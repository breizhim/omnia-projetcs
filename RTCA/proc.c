//------------------------------------------------------------------------------
// Projet RtCA          : Read to Catch All
// Auteur               : Nicolas Hanteville
// Site                 : http://code.google.com/p/omnia-projetcs/
// Licence              : GPL V3
//------------------------------------------------------------------------------
#include "RtCA.h"

#define DNS_MALWARE_MIN_SIZE  4
//------------------------------------------------------------------------------
DWORD WINAPI UpdateRtCA_Thread(LPVOID lParam)
{
  update_thread_start = 1;

  //get current date
  time_t date;
  time(&date);
  struct tm *today = localtime(&date);

  //get date
  char date_today[DATE_SIZE_MAX]="";
  strftime(date_today, DATE_SIZE_MAX,"%Y/%m/%d %H:%M:%S",today);
//---------------------------
//update malware database
//http://www.selectrealsecurity.com/public-block-lists
  SendMessage(hstatus_bar,SB_SETTEXT,0, (LPARAM)cps[TXT_UPDATE_START].c);
  //init database ?
  //sqlite3_exec(db_scan,"DELETE from malware_dns_list;", NULL, NULL, NULL);

  //ddl malware file https://easylist-downloads.adblockplus.org/malwaredomains_full.txt
  //init SSL connexion
  HINTERNET M_connexion = 0;
  if (!use_other_proxy)M_connexion = InternetOpen("",/*INTERNET_OPEN_TYPE_DIRECT*/INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, INTERNET_FLAG_NO_CACHE_WRITE);
  else M_connexion = InternetOpen("",/*INTERNET_OPEN_TYPE_DIRECT*/INTERNET_OPEN_TYPE_PROXY, proxy_ch_auth, NULL, 0);

  if (M_connexion==NULL)M_connexion = InternetOpen("",INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, INTERNET_FLAG_NO_CACHE_WRITE);
  if (M_connexion==NULL)return 0;

  if(!SQLITE_FULL_SPEED)sqlite3_exec(db_scan,"BEGIN TRANSACTION;", NULL, NULL, NULL);
  //---------------------------
  HINTERNET M_session = InternetConnect(M_connexion, "easylist-downloads.adblockplus.org",443,"","",INTERNET_SERVICE_HTTP,0,0);
  if (M_session!=NULL)
  {
    //connexion
    HINTERNET M_requete = HttpOpenRequest(M_session,"GET","/malwaredomains_full.txt",NULL,"https://easylist-downloads.adblockplus.org",NULL,
                                          INTERNET_FLAG_NO_CACHE_WRITE|INTERNET_FLAG_SECURE
                                          |INTERNET_FLAG_IGNORE_CERT_CN_INVALID|INTERNET_FLAG_IGNORE_CERT_DATE_INVALID,0);
    if (use_proxy_advanced_settings)
    {
      InternetSetOption(M_requete,INTERNET_OPTION_PROXY_USERNAME,proxy_ch_user,sizeof(proxy_ch_user));
      InternetSetOption(M_requete,INTERNET_OPTION_PROXY_PASSWORD,proxy_ch_password,sizeof(proxy_ch_password));
    }

    if (HttpSendRequest(M_requete, NULL, 0, NULL, 0))
    {
      char *res = malloc(DIXM); //10MO
      memset(res,0,DIXM);
      if (res != NULL)
      {
        INTERNET_BUFFERS ib;
        ib.dwStructSize       = sizeof(INTERNET_BUFFERS);
        ib.lpcszHeader        = NULL;
        ib.dwHeadersLength    = 0;
        ib.dwHeadersTotal     = 0;
        ib.dwOffsetLow        = 0;
        ib.dwOffsetHigh       = 0;
        ib.lpvBuffer          = res;
        ib.dwBufferLength     = DIXM-1;
        ib.dwBufferTotal      = DIXM-1;

        if(InternetReadFileEx(M_requete,&ib,IRF_NO_WAIT,0))
        {
          if (strlen(res)>0)
          {
            //working with file and update
            char request[MAX_LINE_SIZE], domain[MAX_PATH], *c = res, *d;
            do
            {
              //get data by line
              if (*c++ == '|')
              {
                if (*c++ == '|')
                {
                  d = domain;
                  while (*c && *c!='^' && (d-domain < MAX_PATH)) *d++ = *c++;
                  *d = 0;

                  if (strlen(domain)>=DNS_MALWARE_MIN_SIZE)
                  {
                    snprintf(request,MAX_LINE_SIZE,"INSERT INTO malware_dns_list (domain,description,update_time) "
                                                   "VALUES(\"%s\",\"https://easylist-downloads.adblockplus.org/malwaredomains_full.txt\",\"%s\");",domain,date_today);
                    //MessageBox(NULL,"OK",request,MB_OK|MB_TOPMOST);
                    sqlite3_exec(db_scan,request, NULL, NULL, NULL);
                    SendMessage(hstatus_bar,SB_SETTEXT,1, (LPARAM)domain);
                  }

           //next
                  while (*c && *c != '\n')c++;
                }else while (*c && *c != '\n')c++;
              }else while (*c && *c != '\n')c++;
              if (*c == '\n')c++;
            }while (*c);
          }
        }
        free(res);
      }
    }
    SendMessage(hstatus_bar,SB_SETTEXT,1, (LPARAM)"OK : https://easylist-downloads.adblockplus.org/malwaredomains_full.txt");
  }
  //---------------------------
  //http://malc0de.com/bl/BOOT
  M_session = InternetConnect(M_connexion, "malc0de.com",80,"","",INTERNET_SERVICE_HTTP,0,0);
  if (M_session!=NULL)
  {
    //connexion
    HINTERNET M_requete = HttpOpenRequest(M_session,"GET","/bl/BOOT",NULL,"http://malc0de.com",NULL,
                                          INTERNET_FLAG_NO_CACHE_WRITE,0);

    if (use_proxy_advanced_settings)
    {
      InternetSetOption(M_requete,INTERNET_OPTION_PROXY_USERNAME,proxy_ch_user,sizeof(proxy_ch_user));
      InternetSetOption(M_requete,INTERNET_OPTION_PROXY_PASSWORD,proxy_ch_password,sizeof(proxy_ch_password));
    }

    if (HttpSendRequest(M_requete, NULL, 0, NULL, 0))
    {
      char *res = malloc(DIXM); //10MO
      memset(res,0,DIXM);
      if (res != NULL)
      {
        INTERNET_BUFFERS ib;
        ib.dwStructSize       = sizeof(INTERNET_BUFFERS);
        ib.lpcszHeader        = NULL;
        ib.dwHeadersLength    = 0;
        ib.dwHeadersTotal     = 0;
        ib.dwOffsetLow        = 0;
        ib.dwOffsetHigh       = 0;
        ib.lpvBuffer          = res;
        ib.dwBufferLength     = DIXM-1;
        ib.dwBufferTotal      = DIXM-1;

        if(InternetReadFileEx(M_requete,&ib,IRF_NO_WAIT,0))
        {
          if (strlen(res)>323)//bypass 323 first caracts
          {
            //working with file and update
            char request[MAX_LINE_SIZE], domain[MAX_PATH], *c = res+323, *d;
            do
            {
              //get data by line
              //PRIMARY duote.com.cn blockeddomain.hosts

              while(*c && *c!=' ')c++;
              if (*c==' ')
              {
                c++;
                d = domain;
                while ((d-domain < MAX_PATH) && *c && *c!=' ') *d++ = *c++;
                *d = 0;

                if (strlen(domain)>=DNS_MALWARE_MIN_SIZE)
                {
                  snprintf(request,MAX_LINE_SIZE,"INSERT INTO malware_dns_list (domain,description,update_time) "
                                                 "VALUES(\"%s\",\"http://malc0de.com/bl/BOOT\",\"%s\");",domain,date_today);
                  sqlite3_exec(db_scan,request, NULL, NULL, NULL);
                  SendMessage(hstatus_bar,SB_SETTEXT,1, (LPARAM)domain);
                }

                //next line
                while (*c && *c != '\n')c++;
                if (*c == '\n')c++;
              }else break;
            }while (*c);
          }
        }
        free(res);
      }
    }
    SendMessage(hstatus_bar,SB_SETTEXT,1, (LPARAM)"OK : http://malc0de.com/bl/BOOT");
  }
  //---------------------------
  //http://www.malwaredomainlist.com/hostslist/hosts.txt
  M_session = InternetConnect(M_connexion, "www.malwaredomainlist.com",80,"","",INTERNET_SERVICE_HTTP,0,0);
  if (M_session!=NULL)
  {
    //connexion
    HINTERNET M_requete = HttpOpenRequest(M_session,"GET","/hostslist/hosts.txt",NULL,"http://www.malwaredomainlist.com",NULL,
                                          INTERNET_FLAG_NO_CACHE_WRITE,0);
    if (use_proxy_advanced_settings)
    {
      InternetSetOption(M_requete,INTERNET_OPTION_PROXY_USERNAME,proxy_ch_user,sizeof(proxy_ch_user));
      InternetSetOption(M_requete,INTERNET_OPTION_PROXY_PASSWORD,proxy_ch_password,sizeof(proxy_ch_password));
    }

    if (HttpSendRequest(M_requete, NULL, 0, NULL, 0))
    {
      char *res = malloc(DIXM); //10MO
      memset(res,0,DIXM);
      if (res != NULL)
      {
        INTERNET_BUFFERS ib;
        ib.dwStructSize       = sizeof(INTERNET_BUFFERS);
        ib.lpcszHeader        = NULL;
        ib.dwHeadersLength    = 0;
        ib.dwHeadersTotal     = 0;
        ib.dwOffsetLow        = 0;
        ib.dwOffsetHigh       = 0;
        ib.lpvBuffer          = res;
        ib.dwBufferLength     = DIXM-1;
        ib.dwBufferTotal      = DIXM-1;

        if(InternetReadFileEx(M_requete,&ib,IRF_NO_WAIT,0))
        {
          DWORD sz = strlen(res);
          if (sz>207) //bypass 206 first caracts
          {
            //working with file and update
            char request[MAX_LINE_SIZE], domain[MAX_PATH], *c = res+206, *d;
            do
            {
              //get data by line
              //127.0.0.1  0koryu0.easter.ne.jp
              d = domain;
              while ((d-domain < MAX_PATH) && *c && *c!='\r' && *c!='\n') *d++ = *c++;
              *d = 0;

              if (strlen(domain)>=DNS_MALWARE_MIN_SIZE)
              {
                snprintf(request,MAX_LINE_SIZE,"INSERT INTO malware_dns_list (domain,description,update_time) "
                                               "VALUES(\"%s\",\"http://www.malwaredomainlist.com/hostslist/hosts.txt\",\"%s\");",domain,date_today);
                sqlite3_exec(db_scan,request, NULL, NULL, NULL);
                SendMessage(hstatus_bar,SB_SETTEXT,1, (LPARAM)domain);
              }

              //next datas
              while (*c && *c != ' ')c++;
              if (*c == ' ')c++;
              if (*c == ' ')c++;
            }while (*c);
          }
        }
        free(res);
      }
    }
    SendMessage(hstatus_bar,SB_SETTEXT,1, (LPARAM)"OK : http://www.malwaredomainlist.com/hostslist/hosts.txt");
  }
  //---------------------------
  InternetCloseHandle(M_connexion);
  SendMessage(hstatus_bar,SB_SETTEXT,0, (LPARAM)cps[TXT_UPDATE_END].c);
  SendMessage(hstatus_bar,SB_SETTEXT,1, (LPARAM)"");
  if(!SQLITE_FULL_SPEED)sqlite3_exec(db_scan,"END TRANSACTION;", NULL, NULL, NULL);
  update_thread_start = 0;
  return 0;
}
//------------------------------------------------------------------------------
void UpdateRtCA()
{
  if (update_thread_start)return;
  CreateThread(NULL,0,UpdateRtCA_Thread,NULL,0,0);
}
//------------------------------------------------------------------------------
BOOL FileExist(char *file)
{
  if (GetFileAttributes(file) != INVALID_FILE_ATTRIBUTES)return TRUE;
  else return FALSE;
}
//------------------------------------------------------------------------------
void ReviewWOW64Redirect(PVOID OldValue_W64b)
{
  typedef BOOL (WINAPI *WOW64DISABLEREDIRECT)(PVOID *OldValue);

  HMODULE hDLL = LoadLibrary( "KERNEL32.dll");
  if (hDLL != NULL)
  {
    if (OldValue_W64b == FALSE)
    {
      WOW64DISABLEREDIRECT Wow64DisableWow64FsRedirect = (BOOL (WINAPI *)(PVOID *OldValue)) GetProcAddress(hDLL, "Wow64DisableWow64FsRedirection");
      if (Wow64DisableWow64FsRedirect)Wow64DisableWow64FsRedirect(&OldValue_W64b);
    }else
    {
      WOW64DISABLEREDIRECT Wow64RevertWow64FsRedirect  = (BOOL (WINAPI *)(PVOID *OldValue)) GetProcAddress(hDLL, "Wow64RevertWow64FsRedirection");
      if (Wow64RevertWow64FsRedirect)Wow64RevertWow64FsRedirect(&OldValue_W64b);
    }
    FreeLibrary(hDLL);
  }
}
//------------------------------------------------------------------------------
void replace_one_char(char *buffer, unsigned long int taille, char chtoreplace, char chreplace)
{
  char *c = buffer;

  while (c != buffer+taille)
  {
    if (*c == chtoreplace) *c = chreplace;
    c++;
  }
}
//------------------------------------------------------------------------------
void replace_to_char(char *buffer, unsigned long int taille, char a)
{
  char *c = buffer;

  while (c != buffer+taille-4)
  {
    if (*c==0x00 && *(c+1)==0x00 && *(c+2)==0x00&& *(c+3)==0x00)break;
    else if (*c==0x00 && *(c+1)==0x00) *c = a;
    c++;
  }
}
//------------------------------------------------------------------------------
char *convertStringToSQL(char *data, unsigned int size_max)
{
  char tmp[MAX_PATH];
  char *s = data;
  char *d = tmp;
  unsigned int i=0, j=0;
  for (;j<size_max && i<MAX_PATH;i++,j++,d++,s++)
  {
    if (*s == '"')
    {
      if (j+2 < MAX_PATH)
      {
        *d++ = '"';j++;
        *d = '"';
      }else *d = '\'';
    }else *d = *s;
  }
  *d = 0;

  snprintf(data,size_max,"%s",tmp);
  return data;
}
//------------------------------------------------------------------------------
char *ReplaceEnv(char *var, char *path, unsigned int size_max)
{
  char tmp[MAX_LINE_SIZE]="";
  if (ExpandEnvironmentStrings(path,tmp,MAX_LINE_SIZE))
  {
    strncpy(path,tmp,size_max);
    return path;
  }else
  {
    //get var
    if (getenv(var) && path[0]=='%')
    {
      char *c = path;
      while (*c && *c!='\\')c++;
      if (*c == '\\')
      {
        char tmp[MAX_PATH];
        snprintf(tmp,MAX_PATH,"%s%s",getenv(var),c);
        strncpy(path,tmp,size_max);
      }
    }
    return path;
  }
}
//------------------------------------------------------------------------------
char *GetTextFromTrv(HTREEITEM hitem, char *txt, DWORD item_size_max)
{
  TVITEM tvitem;
  txt[0]=0;
  tvitem.hItem = hitem;
  tvitem.mask = TVIF_TEXT;
  tvitem.pszText = txt;
  tvitem.cchTextMax = item_size_max;
  SendMessage(htrv_files, TVM_GETITEM,(WPARAM)0, (LPARAM)&tvitem);
  return txt;
}
//------------------------------------------------------------------------------
int GetTrvItemIndex(HTREEITEM hitem, HANDLE htrv)
{
  int index =0;
  HTREEITEM item = hitem;

  while ((item = (HTREEITEM)SendMessage(htrv,TVM_GETNEXTITEM,(WPARAM)TVGN_PREVIOUS, (LPARAM)item)))index++;

  return index;
}
//------------------------------------------------------------------------------
BOOL SetDebugPrivilege(BOOL enable)
{
    TOKEN_PRIVILEGES privilege;
    LUID Luid;
    HANDLE handle1;
    HANDLE handle2;

    handle1 = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
    OpenProcessToken(handle1,TOKEN_ALL_ACCESS, &handle2);
    LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &Luid);
    privilege.PrivilegeCount = 1;
    privilege.Privileges[0].Luid = Luid;
    privilege.Privileges[0].Attributes = enable?SE_PRIVILEGE_ENABLED:0x04/*SE_PRIVILEGE_REMOVED*/;
    BOOL ret = AdjustTokenPrivileges(handle2, FALSE, &privilege, sizeof(privilege), NULL, NULL);

    CloseHandle(handle2);
    CloseHandle(handle1);
    return ret;
}
//------------------------------------------------------------------------------
char *filetimeToString(FILETIME FileTime, char *str, unsigned int string_size)
{
  str[0] = 0;
  SYSTEMTIME SysTime;
  if (FileTimeToSystemTime(&FileTime, &SysTime) != 0)//traitement de l'affichage de la date
    snprintf(str,string_size,"%02d/%02d/%02d %02d:%02d:%02d",SysTime.wYear,SysTime.wMonth,SysTime.wDay,SysTime.wHour,SysTime.wMinute,SysTime.wSecond);

  return str;
}
//------------------------------------------------------------------------------
char *filetimeToString_GMT(FILETIME FileTime, char *str, unsigned int string_size)
{
  str[0] = 0;
  if (UTC_TIME)filetimeToString(FileTime, str, string_size);
  else
  {
    FILETIME LocalFileTime;
    if(FileTimeToLocalFileTime(&FileTime, &LocalFileTime))
      filetimeToString(LocalFileTime, str, string_size);
  }

  if (str[0] == '1' && str[1] == '6')str[0] = 0;
  return str;
}
//------------------------------------------------------------------------------
char *timeToString(DWORD t, char *str, unsigned int string_size)
{
  FILETIME FileTime,LocalFileTime;
  LONGLONG lgTemp = Int32x32To64(t,10000000) + 116444736000000000;

  FileTime.dwLowDateTime  = (DWORD) lgTemp;
  FileTime.dwHighDateTime = (DWORD)(lgTemp >> 32);

  if (UTC_TIME)
  {
    filetimeToString(FileTime, str, string_size);
  }else
  {
    if (FileTimeToLocalFileTime(&FileTime, &LocalFileTime))
    {
      filetimeToString(LocalFileTime, str, string_size);
    }
  }

  return str;
}
//------------------------------------------------------------------------------
char *convertUTF8toUTF16(char *src, DWORD size)
{
  if (src == NULL && size == 0)return src;

  wchar_t *buffer;
  buffer = malloc(sizeof(wchar_t)*size);
  if (buffer)
  {
    MultiByteToWideChar(CP_UTF8, 0, src, size, buffer, size);
    snprintf(src,size,"%S",buffer);
  }
  free(buffer);
  return src;
}
//------------------------------------------------------------------------------
char *convertUTF8toUTF16toChar(char *src, DWORD size_src, char *dst, DWORD size_dst)
{
  if (src == NULL || size_src == 0)return NULL;

  wchar_t *buffer;
  buffer = malloc(sizeof(wchar_t)*size_src+1);
  if (buffer)
  {
    MultiByteToWideChar(CP_UTF8, 0, src, size_src, buffer, size_src);
    snprintf(dst,size_dst,"%S",buffer);
  }
  free(buffer);
  return dst;
}
//------------------------------------------------------------------------------
char *charToLowChar(char *src)
{
  unsigned int i;
  for (i=0;i<strlen(src);i++)
  {
    if (src[i]>64 && src[i]<91)src[i] = src[i]+32;
  }
  return src;
}
//------------------------------------------------------------------------------
char *DataToHexaChar(char *data, unsigned int data_size, char *hexa_char, unsigned int hexa_char_size)
{
  if (data_size >0 && hexa_char_size > 0 && data!=NULL && hexa_char!=NULL)
  {
    unsigned int i;
    for (i=0;i<data_size && i<(hexa_char_size/2);i++)
    {
      snprintf(hexa_char+strlen(hexa_char),hexa_char_size-strlen(hexa_char),"%02X",data[i]&0xff);
    }
  }
  return hexa_char;
}
//------------------------------------------------------------------------------
char *extractDirectoryFromPath(char *path)
{
  char *c = path;

  while(*c++);
  while(*c!='\\' && *c!='/' && c>path)c--;
  *c = 0;

  return path;
}
//------------------------------------------------------------------------------
char *extractFileFromPath(char *path, char *file, unsigned int file_size_max)
{
  char *c = path;
  file[0] = 0;

  while(*c++);
  while(*c!='\\' && *c!='/' && c>path)c--;
  c++;
  strncpy(file,c,file_size_max);
  return file;
}
//------------------------------------------------------------------------------
char *extractExtFromFile(char *file, char *ext, unsigned int ext_size_max)
{
  char *c = file;
  ext[0]  = 0;

  while(*c++);
  while(*c!='\\' && *c!='/' && *c!='.' && c>file)c--;
  if (*c == '.')
  {
    c++;
    strncpy(ext,c,ext_size_max);
    charToLowChar(ext);
    return ext;
  }else return NULL;
}
//------------------------------------------------------------------------------
BOOL isDirectory(char *path)
{
  DWORD attr = GetFileAttributes(path);
  if (attr & FILE_ATTRIBUTE_DIRECTORY) return TRUE;
  else return FALSE;
}
//------------------------------------------------------------------------------
BOOL startWith(char* txt, char *search)
{
  char *t = txt, *s = search;

  while (*t && *s && *t == *s){t++;s++;}
  if (*t == *s || *s == 0)return TRUE;
  return FALSE;
}
//------------------------------------------------------------------------------
BOOL isWine()
{
  HKEY CleTmp=0;
  if (RegOpenKey(HKEY_LOCAL_MACHINE,"Software\\Wine",&CleTmp)==ERROR_SUCCESS)
  {
    RegCloseKey(CleTmp);
    return TRUE;
  }

  //deuxi�me cas
  char tmp[MAX_PATH]="";
  if(ReadValue(HKEY_LOCAL_MACHINE,"Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug","Debugger",tmp, MAX_PATH))
  {
    if (Contient(tmp,"winedbg"))
    {
      return TRUE;
    }
  }

  return FALSE;
}
