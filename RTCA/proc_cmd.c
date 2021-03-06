//------------------------------------------------------------------------------
// Projet RtCA          : Read to Catch All
// Auteur               : Nicolas Hanteville
// Site                 : http://code.google.com/p/omnia-projetcs/
// Licence              : GPL V3
//------------------------------------------------------------------------------
#include "RtCA.h"
//------------------------------------------------------------------------------
int callback_sqlite_CMD(void *datas, int argc, char **argv, char **azColName)
{
  FORMAT_CALBAK_TYPE *type = datas;
  switch(type->type)
  {
    case CMD_TYPE_LIST_TESTS:
    case CMD_TYPE_LIST_SESSION:
      printf("\t[%02d] - %s\n",atoi(argv[0]),argv[1]);
    break;
    case CMD_TYPE_LIST_LANGUAGE:
      printf("\t[%02d] - %s\n",item_n++,argv[1]);
    break;
    //case TYPE_SQL_ADD_SESSION:break;
    case CMD_READ_SESSION:
      current_session_id = atoi(argv[0]);
    break;
  }
  return 0;
}
//------------------------------------------------------------------------------
void AddNewSession(BOOL local_only, sqlite3 *db)
{
  //generate name : date_computer_name
  time_t date;
  time(&date);
  struct tm *today = localtime(&date);
  char request[MAX_LINE_SIZE];

  //get date
  char date_today[DATE_SIZE_MAX];
  strftime(date_today, DATE_SIZE_MAX,"%Y/%m/%d %H:%M:%S",today);

  //session number = seconde after 01/01/2012 00:00:00
  DWORD id = (today->tm_year - 2012)*60*60*24*365 + (today->tm_mon - 01)*60*60*24*30 + (today->tm_mday - 01)*60*60*24 + today->tm_hour*60*60 + today->tm_min*60 + today->tm_sec;

  //if with local only or files:
  if (local_only)
  {
    //get local name
    char computer_name[COMPUTER_NAME_SIZE_MAX];
    DWORD taille = COMPUTER_NAME_SIZE_MAX;
    GetComputerName(computer_name,&taille);

    if (UTC_TIME)snprintf(request, MAX_LINE_SIZE,"INSERT INTO session (id,name) VALUES(%lu,\"%s_(UTC)_%s\");",id,date_today,computer_name);
    else snprintf(request, MAX_LINE_SIZE,"INSERT INTO session (id,name) VALUES(%lu,\"%s_%s\");",id,date_today,computer_name);
  }else
  {
    if (UTC_TIME)snprintf(request, MAX_LINE_SIZE,"INSERT INTO session (id,name) VALUES(%lu,\"%s_(UTC)_files\");",id,date_today);
    else snprintf(request, MAX_LINE_SIZE,"INSERT INTO session (id,name) VALUES(%lu,\"%s_files\");",id,date_today);
  }

  //add session
  FORMAT_CALBAK_READ_INFO fcri;
  fcri.type  = TYPE_SQL_ADD_SESSION;
  sqlite3_exec(db,request, callback_sqlite_CMD, &fcri, NULL);

  //read the last session
  current_session_id = id;
}

