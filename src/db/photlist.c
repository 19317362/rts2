/*!
 * Pohotometer list in VYPAR (Harmanec and al.) B format
 */

#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <time.h>
#include <stdlib.h>
#include <getopt.h>
#include "libpq-fe.h"

PGconn *conn;

static void
exit_nicely ()
{
  PQfinish (conn);
  exit (1);
}

int night_day = 0;
int night_month = 0;
int night_year = 0;
int target = 0;

#define PQ_EXEC(command) \
      res = PQexec(conn, command); \
      if (PQresultStatus(res) != PGRES_COMMAND_OK) \
	{ \
                fprintf(stderr, "%s command failed: %s", command, PQerrorMessage(conn)); \
                PQclear(res); \
		return; \
	}

void
print_head (struct tm *date)
{
  printf
    ("FRAM photometry\nPierre-Auger observatory, Argentina, South America\n");
  printf ("%3i%3i%5i 426 0\n", date->tm_mday + 1, date->tm_mon + 1,
	  date->tm_year + 1900);
  printf ("  10.000 20.300 120\n");
}

void
get_list (int tar_id)
{
  int nFields;
  int i;

  time_t count_date;
  struct tm date;
  uint64_t count_value;
  char count_filter[3];
  char *command;
  char sel[2000] =
    "SELECT "
    "  tar_id,"
    "  MIN(count_date) as cd,"
    "  int8(AVG(count_value / count_exposure)),"
    "  count_filter " "FROM" "  targets_counts ";
  PGresult *res;
// d d y b v u
  int filters[] = { 0, 0, 4, 2, 1, 3 };
  time_t next_midnight = 0;

  command = "BEGIN";

  PQ_EXEC (command);
  PQclear (res);

  if (target)
    {
      sprintf (sel, "%s WHERE tar_id = %i ", sel, target);
    }

  strcat (sel,
	  "GROUP BY"
	  "  tar_id, count_filter, obs_id " "ORDER BY" "  cd ASC;");

  res = PQexecParams (conn, sel, 0, NULL, NULL, NULL, NULL, 1);
  if (PQresultStatus (res) != PGRES_TUPLES_OK)
    {
      fprintf (stderr, "SELECT failed: %s", PQerrorMessage (conn));
      PQclear (res);
      return;
    }
  nFields = PQnfields (res);
  /* next, print out the rows */
  for (i = 0; i < PQntuples (res); i++)
    {
      tar_id = ntohl (*((uint32_t *) PQgetvalue (res, i, 0)));
      count_date = ntohl (*((uint32_t *) (PQgetvalue (res, i, 1))));
      gmtime_r (&count_date, &date);
      count_value = ntohl (*((uint32_t *) PQgetvalue (res, i, 2)));
      count_value <<= 32;
      count_value |= ntohl (*(((uint32_t *) PQgetvalue (res, i, 2)) + 1));
      strncpy (count_filter, PQgetvalue (res, i, 3), 3);
      if (next_midnight < count_date)
	{
	  print_head (&date);
	  next_midnight = count_date + abs ((date.tm_hour - 4) % 24) * 3600;
	}
      printf ("%1i%4i%3i%3i%4i.0000%10lli\n", filters[count_filter[0] - 'A'],
	      tar_id, date.tm_hour, date.tm_min, date.tm_sec, count_value);
    }

  PQclear (res);
  PQ_EXEC (command);
  PQclear (res);
  return;
}

int
main (int argc, char **argv)
{
  const char *conninfo;
  int c;

  conninfo = "dbname = stars";

  conn = PQconnectdb (conninfo);

  if (PQstatus (conn) != CONNECTION_OK)
    {
      fprintf (stderr, "Connection to database '%s' failed.\n", PQdb (conn));
      fprintf (stderr, "%s", PQerrorMessage (conn));
      exit_nicely ();
    }
  while (1)
    {
      static struct option long_option[] = {
	{"start_day", 1, 0, 'd'},
	{"start_month", 1, 0, 'm'},
	{"start_year", 1, 0, 'y'},
	{"target", 1, 0, 't'},
	{0, 0, 0, 0}
      };
      c = getopt_long (argc, argv, "d:m:y:t:", long_option, NULL);
      if (c == -1)
	break;
      switch (c)
	{
	case 'd':
	  night_day = atoi (optarg);
	  break;
	case 'm':
	  night_month = atoi (optarg);
	  break;
	case 'y':
	  night_year = atoi (optarg);
	  break;
	case 't':
	  target = atoi (optarg);
	  break;
	case '?':
	  break;
	default:
	  printf ("?? getopt ret: %o\n", c);
	}
    }

  get_list (120);

  /* close the connection to the database and cleanup */
  PQfinish (conn);

  return 0;

}
