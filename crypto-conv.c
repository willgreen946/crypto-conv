#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define ARRAY_MAX(x) (sizeof(x) / sizeof(x[0]))

static int help(const char *);
static int which_coin(const char *);
static int which_fiat(const char *);
static double str_to_double(const char *);
static char *get_page(const char *);
size_t write_page_data(void *, size_t, size_t, void *);
static char *append_str(char *, char *);
static double json_get_price(int, int, const char *);
static void display_price(int, int, double);
static void display_holdings(int, int, double, double);

struct page_data {
  char *buf;
  size_t len;
};

struct fiat_map {
  char *name;
  char *symbol;
  int enum_val;
};

enum {
  GBP,
  USD,
  EUR,
};

const struct fiat_map f_map[] = {
  { "gbp", "\u00a3", GBP },
  { "usd", "\u0024", USD },
  { "eur", "\u20ac", EUR },
};

struct coin_map {
  char *symbol;
  char *full;
  int enum_val;
  char *url;
};

enum {
  BTC,
  XMR,
  LTC,
  ETH
};

const struct coin_map c_map[] = {
  /* the url's are incomplete and need a fiat currency added to the end i.e "vs_currencies=usd" */
  { "btc", "bitcoin", BTC, "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=" },
  { "xmr", "monero", XMR, "https://api.coingecko.com/api/v3/simple/price?ids=monero&vs_currencies=" },
  { "ltc", "litecoin", LTC, "https://api.coingecko.com/api/v3/simple/price?ids=litecoin&vs_currencies=" },
  { "eth", "ethereum", ETH, "https://api.coingecko.com/api/v3/simple/price?ids=ethereum&vs_currencies=" },
};

int opt_ctf           = 1; /* crypto->fiat */
int opt_holdings      = 0; /* calculate price of holdings (if true) */
int opt_display_price = 0; /* display price of the crypto currency */

int
main(int argc, char **argv)
{
  int coin, fiat;
  char *json, *url;
  double price, holdings;

  coin = 0;
  fiat = USD;
  price = holdings = 0.0f;
  json = url = (char *) 0;

  if (argc < 2 || argc > 5)
    return help(argv[0]);

  if (argc == 2 || argc == 3)
    opt_display_price = 1;

  /* get the number associated with the coin we will be using */
  if ((coin = which_coin(argv[1])) < 0)
    return help(argv[0]);

  /* determine what fiat currency the user wants to display */
  if (argc >= 3)
    if ((fiat = which_fiat(argv[2])) < 0)
      return help(argv[0]);

  /* get the amount of crypto the user holds */
  if (argc >= 4) {
    if ((holdings = str_to_double(argv[3])) < 0)
      return help(argv[0]);

    opt_holdings = 1;
  }

  /* fiat->crypto or crypto->fiat */
  if (argc == 5) {
    if (!strcmp(argv[4], "ftc"))
      opt_ctf = 0;
    else if (!strcmp(argv[4], "ctf"))
      opt_ctf = 1;
    else {
      fprintf(stderr, "%s : no such conversion \"%s\"\n", __func__, argv[4]);
      return help(argv[0]);
    }
  }

  /* append the fiat currency to the url */
  url = append_str(c_map[coin].url, f_map[fiat].name);  
  
  /* get the price data in json format from coin gecko */
  if (!(json = get_page(url)))
    return -1;

  free(url);

  price = json_get_price(coin, fiat, json);

  free(json);

  if (opt_display_price == 1) {
    display_price(coin, fiat, price);
    return 0;
  } else if (opt_holdings == 1) {
    display_holdings(coin, fiat, holdings, price);
    return 0; 
  }

  return 0;
}

static int
help(const char *cmd)
{
  printf("Usage:\n%s <coin> <fiat> <amount> <conversion>\n\n", cmd);
  printf("Example: %s xmr gbp 360 ftc\n", cmd);
  puts("This will convert 360 Great British Pounds to Monero\n");
  puts("Valid conversions are:\nftc (fiat -> crypto)\nctf (crypto -> fiat)\n");
  puts("Available coins:");

  for (int i = 0; i < (int) ARRAY_MAX(c_map); i++)
    puts(c_map[i].symbol);

  printf("\nAvailable fiat currencies:\n");

  for (int i = 0; i < (int) ARRAY_MAX(f_map); i++)
    puts(f_map[i].name);

  return 0;
}

static int
which_coin(const char *str)
{
  /* will need to increase the limit to use coins like DOGE */
  if (strlen(str) > 3)  
    goto fail;

  for (int i = 0; i < (int) ARRAY_MAX(c_map); i++)
    if (!strncmp(str, c_map[i].symbol, 3))
      return c_map[i].enum_val; /* return the enum value of the coin */

fail:
  fprintf(stderr, "%s : Invalid coin \"%s\"\n", __func__, str);
  return -1;
}

static int
which_fiat(const char *str)
{
  if (strlen(str) != 3)  
    goto fail;

  for (int i = 0; i < (int) ARRAY_MAX(f_map); i++)
    if (!strncmp(str, f_map[i].name, 3))
      return f_map[i].enum_val; /* return the enum value of the coin */

fail:
  fprintf(stderr, "%s : Invalid fiat currency \"%s\"\n", __func__, str);
  return -1;
}

static double
str_to_double(const char *buf)
{
  /* TODO maybe do some checks on buf to see if its a valid double */
  return strtod(buf, (char **) 0);
}

static char *
get_page(const char *url)
{
  CURL *c_handle;
  CURLcode c_result;
  struct page_data p_data;

  if (!url)
    return (char *) 0;

  p_data.buf = (char *) malloc(1);
  p_data.len = 0;

  c_handle = curl_easy_init();

  if (c_handle) {
    curl_easy_setopt(c_handle, CURLOPT_URL, url);
    curl_easy_setopt(c_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c_handle, CURLOPT_WRITEFUNCTION, write_page_data);
    curl_easy_setopt(c_handle, CURLOPT_WRITEDATA, (void *) &p_data);
    curl_easy_setopt(c_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    c_result = curl_easy_perform(c_handle);

    if (c_result != CURLE_OK)
      fprintf(stderr, "%s error : %s\n", __func__, curl_easy_strerror(c_result));

    curl_easy_cleanup(c_handle);

    return p_data.buf;
  }

  return (char *) 0;
}

size_t
write_page_data(void *data, size_t len, size_t n_mem, void *user_ptr)
{ 
  char *buf;
  size_t actual_len;
  struct page_data *p_data;

  actual_len = len * n_mem;
  p_data = (struct page_data *) user_ptr;

  if (!(buf = realloc(p_data->buf, p_data->len + actual_len + 1))) {
    fprintf(stderr, "%s : Failed to allocate memory\n", __func__);
    return 0;
  }

  p_data->buf = buf;
  memcpy(&(p_data->buf[p_data->len]), data, actual_len);
  p_data->len += actual_len;
  p_data->buf[p_data->len] = 0;

  return actual_len;
}

static char *
append_str(char *to, char *from)
{
  char *new;
  size_t n_len;

  n_len = strlen(to) + strlen(from) + 1;

  if (!(new = (char *) malloc(n_len))) {
    fprintf(stderr, "%s error: Failed to allocate memory\n", __func__);
    return new;
  }

  new[0] = (char) 0;
  strlcat(new, to, n_len);
  strlcat(new, from, n_len);

  return new;
}

static double
json_get_price(int coin_d, int fiat_d, const char *buf)
{
  /*
   * really rigid function for getting the price of the crypto
   * I say rigid as its going off the assumption that coin gecko
   * will return something like "{"bitcoin":{"usd":43123}}"
   * not dynamic at all and will probably need work in future as things change
   */
  cJSON *root;
  double val = 0.0f;

  if (!(root = cJSON_Parse(buf)))
    return val;

  cJSON *coin = cJSON_GetObjectItem(root, c_map[coin_d].full);
  cJSON *fiat = cJSON_GetObjectItem(coin, f_map[fiat_d].name); 
  
  if (cJSON_IsNumber(fiat)) {
    val = fiat->valuedouble;
  }

  return val;
}

static void
display_price(int coin, int fiat, double price)
{
  fprintf(stdout, "1 %s = %s%0.11f\n",
    c_map[coin].symbol, f_map[fiat].symbol, price);
}

static void
display_holdings(int coin, int fiat, double holdings, double price)
{
  double holdings_price;

  if (opt_ctf == 1) {
    holdings_price = price * holdings;
    fprintf(stdout, "%0.11f %s = %s%0.11f\n",
      holdings, c_map[coin].symbol, f_map[fiat].symbol, holdings_price);
  } else {
    holdings_price = holdings / price;
    fprintf(stdout, "%s%0.11f = %0.11f %s\n",
      f_map[fiat].symbol, holdings, holdings_price, c_map[coin].symbol);
  }
}
