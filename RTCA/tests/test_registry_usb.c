//------------------------------------------------------------------------------
// Projet RtCA          : Read to Catch All
// Auteur               : Nicolas Hanteville
// Site                 : http://code.google.com/p/omnia-projetcs/
// Licence              : GPL V3
//------------------------------------------------------------------------------
#include "../RtCA.h"
//------------------------------------------------------------------------------
void addRegistryUSBtoDB(char *file, char *hk, char *key, char *name,
                             char *vendor_id, char*product_id, char*description,
                             char *pusb, char *lecteur,char *last_use,
                             unsigned int session_id, sqlite3 *db)
{
  #ifndef CMD_LINE_ONLY_NO_DB
  char request[REQUEST_MAX_SIZE];
  snprintf(request,REQUEST_MAX_SIZE,
           "INSERT INTO extract_registry_usb (file,hk,key,name,vendor_id,product_id,description,pusb,lecteur,last_use,session_id) "
           "VALUES(\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%d);",
           file,hk,key,name,vendor_id,product_id,description,pusb,lecteur,last_use,session_id);
  sqlite3_exec(db,request, NULL, NULL, NULL);
  #else
  printf("\"USB\";\"%s\";\"%s\";\"%s\";\"%s\";\"%s\";\"%s\";\"%s\";\"%s\";\"%s\";\"%s\";\"%d\";\r\n",
         file,hk,key,name,vendor_id,product_id,description,pusb,lecteur,last_use,session_id);
  #endif
}
//------------------------------------------------------------------------------
//local function part !!!
//------------------------------------------------------------------------------
void SearchVidPid_local(char *s_key,char *vendor_id,DWORD vendor_id_size,char *product_id,DWORD product_id_size,
                        char *pusb,DWORD pusb_size,char *lecteur,DWORD lecteur_size)
{
  //prepare real key
  DWORD size_key = strlen(s_key);
  if (size_key<4 || product_id_size<5 || vendor_id_size<5)return;

  char v_key[MAX_PATH];
  strncpy(v_key,s_key,MAX_PATH);
  v_key[size_key-2]=0;

  char value[MAX_PATH],data[MAX_PATH];
  DWORD value_size = MAX_PATH,data_size = MAX_PATH;

  //search the vkey
  HKEY CleTmp,CleTmp2;
  if (RegOpenKey(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Enum\\USB",&CleTmp)!=ERROR_SUCCESS)return;

  DWORD i,j,nbSubKey=0,nbSubKey2,key_size,key_size2;
  if (RegQueryInfoKey (CleTmp,0,0,0,&nbSubKey,0,0,0,0,0,0,0)==ERROR_SUCCESS)
  {
    char key[MAX_PATH],key2[MAX_PATH];
    char tmp_key[MAX_PATH],tmp_key2[MAX_PATH],symbolicname[MAX_PATH];

    for (i=0;i<nbSubKey && start_scan;i++)
    {
      key_size  = MAX_PATH;
      key[0]    = 0;
      if (RegEnumKeyEx (CleTmp,i,key,&key_size,0,0,0,0)!=ERROR_SUCCESS)continue;
      if (key_size < 17)continue;

      //sub key
      snprintf(tmp_key,MAX_PATH,"SYSTEM\\CurrentControlSet\\Enum\\USB\\%s",key);

      if (RegOpenKey(HKEY_LOCAL_MACHINE,tmp_key,&CleTmp2)!=ERROR_SUCCESS)continue;
      nbSubKey2 = 0;
      if (RegQueryInfoKey (CleTmp2,0,0,0,&nbSubKey2,0,0,0,0,0,0,0)==ERROR_SUCCESS)
      {
        for (j=0;j<nbSubKey2 && start_scan;j++)
        {
          key_size2 = MAX_PATH;
          key2[0]   = 0;
          if (RegEnumKeyEx (CleTmp2,j,key2,&key_size2,0,0,0,0)==ERROR_SUCCESS)
          {
            if (strcmp(key2,v_key)==0)
            {
              //read VID :
              vendor_id[0] = key[4];
              vendor_id[1] = key[5];
              vendor_id[2] = key[6];
              vendor_id[3] = key[7];
              vendor_id[4] = 0;

              //read PID
              product_id[0] = key[13];
              product_id[1] = key[14];
              product_id[2] = key[15];
              product_id[3] = key[16];
              product_id[4] = 0;

              strncpy(pusb,key,pusb_size);

              //get SymbolicName
              lecteur[0]      = 0;
              symbolicname[0] = 0;
              snprintf(tmp_key2,MAX_PATH,"%s\\%s\\device parameters",tmp_key,key2);
              if (ReadValue(HKEY_LOCAL_MACHINE,tmp_key2,"SymbolicName",symbolicname, MAX_PATH))
              {
                HKEY CleTmp3;
                if (RegOpenKey(HKEY_LOCAL_MACHINE,"SYSTEM\\MountedDevices",&CleTmp3)==ERROR_SUCCESS)
                {
                  //test if one match
                  DWORD nbValue = 0, k;
                  if (RegQueryInfoKey (CleTmp3,0,0,0,0,0,0,&nbValue,0,0,0,0)==ERROR_SUCCESS)
                  {
                    for (k=0;k<nbValue;k++)
                    {
                      value_size = MAX_PATH;data_size = MAX_PATH;
                      value[0] = 0;
                      data[0]  = 0;
                      if (RegEnumValue (CleTmp3,k,value,&value_size,0,0,(LPBYTE)data,&data_size)==ERROR_SUCCESS)
                      {
                        if (value[0] == '\\' && value[3] != '\\')
                        {
                          snprintf(lecteur,lecteur_size,"%S",data);
                          if (!strcmp(lecteur,symbolicname))
                          {
                            strncpy(lecteur,value,lecteur_size);
                            break;
                          }
                        }
                      }
                      lecteur[0] = 0;
                    }
                  }
                  RegCloseKey(CleTmp3);
                }
              }

              RegCloseKey(CleTmp2);
              RegCloseKey(CleTmp);
              return;
            }
          }
        }
      }
      RegCloseKey(CleTmp2);
    }
  }
  RegCloseKey(CleTmp);
}
//------------------------------------------------------------------------------
void EnumUSB_local(HKEY hk, char *s_hk, char *path, unsigned int session_id, sqlite3 *db)
{
  HKEY CleTmp,CleTmp2;
  if (RegOpenKey(hk,path,&CleTmp)!=ERROR_SUCCESS)return;

  DWORD i,j,nbSubKey=0,nbSubKey2,key_size,key_size2;
  if (RegQueryInfoKey (CleTmp,0,0,0,&nbSubKey,0,0,0,0,0,0,0)==ERROR_SUCCESS)
  {
    char key[MAX_PATH],key2[MAX_PATH];
    char tmp_key[MAX_PATH],tmp_key2[MAX_PATH];
    char lastupdate[DATE_SIZE_MAX],name[MAX_PATH],vendor_id[MAX_PATH],product_id[MAX_PATH],
    description[MAX_PATH],description1[MAX_PATH],description2[MAX_PATH],pusb[MAX_PATH],lecteur[MAX_PATH];

    for (i=0;i<nbSubKey && start_scan;i++)
    {
      key_size  = MAX_PATH;
      key[0]    = 0;
      if (RegEnumKeyEx (CleTmp,i,key,&key_size,0,0,0,0)!=ERROR_SUCCESS)continue;

      //sub key
      snprintf(tmp_key,MAX_PATH,"%s\\%s",path,key);
      if (RegOpenKey(hk,tmp_key,&CleTmp2)!=ERROR_SUCCESS)continue;
      nbSubKey2 = 0;
      if (RegQueryInfoKey (CleTmp2,0,0,0,&nbSubKey2,0,0,0,0,0,0,0)==ERROR_SUCCESS)
      {
        for (j=0;j<nbSubKey2 && start_scan;j++)
        {
          key_size2 = MAX_PATH;
          key2[0]   = 0;
          if (RegEnumKeyEx (CleTmp2,j,key2,&key_size2,0,0,0,0)==ERROR_SUCCESS)
          {
            snprintf(tmp_key2,MAX_PATH,"%s\\%s",tmp_key,key2);

            //last update
            lastupdate[0] = 0;
            name[0]       = 0;
            vendor_id[0]  = 0;
            product_id[0] = 0;
            description[0]= 0;
            description1[0]= 0;
            description2[0]= 0;
            pusb[0]       = 0;
            lecteur[0]    = 0;

            if (ReadValue(hk,tmp_key2,"Class",description1, MAX_PATH))
            {
              ReadKeyUpdate(hk,tmp_key2, lastupdate, DATE_SIZE_MAX);

              ReadValue(hk,tmp_key2,"Mfg",description2, MAX_PATH);
              snprintf(description,MAX_PATH,"%s %s (%s)",description1,description2,key);

              ReadValue(hk,tmp_key2,"FriendlyName",name, MAX_PATH);

              SearchVidPid_local(key2,vendor_id,MAX_PATH,product_id,MAX_PATH,pusb,MAX_PATH,lecteur,MAX_PATH);

              convertStringToSQL(description, MAX_PATH);
              addRegistryUSBtoDB("", s_hk, tmp_key2, name, vendor_id, product_id, description, pusb, lecteur,lastupdate,session_id, db);
            }
          }
        }
      }
      RegCloseKey(CleTmp2);
    }
  }
  RegCloseKey(CleTmp);
}
//------------------------------------------------------------------------------
//file registry part
//------------------------------------------------------------------------------
void SearchVidPid_file(HK_F_OPEN *hks, char *ckey, char *s_key, char *vendor_id, DWORD vendor_id_size,
                       char *product_id, DWORD product_id_size,char *pusb, DWORD pusb_size,char *lecteur, DWORD lecteur_size)
{
  //prepare real key
  if(vendor_id != NULL)vendor_id[0] = 0;
  if(product_id != NULL)product_id[0] = 0;

  DWORD size_key = strlen(s_key);
  if (size_key<4 || product_id_size<5 || vendor_id_size<5)return;

  char v_key[MAX_PATH],tmp_key[MAX_PATH],tmp_key2[MAX_PATH],
  symbolicname[MAX_PATH],tmp_key3[MAX_PATH];
  strncpy(v_key,s_key,MAX_PATH);
  v_key[size_key-2]=0;

  char value[MAX_PATH],data[MAX_PATH];
  DWORD value_size = MAX_PATH,data_size = MAX_PATH;

  //exist or not in the file ?
  HBIN_CELL_NK_HEADER *nk_h = GetRegistryNK(hks->buffer, hks->taille_fic, (hks->pos_fhbin)+HBIN_HEADER_SIZE, hks->position, ckey);
  if (nk_h == NULL)return;

  HBIN_CELL_NK_HEADER *nk_h_tmp;
  DWORD i,j,nbSubKey2, nbSubKey = GetSubNK(hks->buffer, hks->taille_fic, nk_h, hks->position, 0, NULL, 0);
  for (i=0;i<nbSubKey && start_scan;i++)
  {
    if(GetSubNK(hks->buffer, hks->taille_fic, nk_h, hks->position, i, tmp_key, MAX_PATH))
    {
      //get nk of key :)
      nk_h_tmp = GetSubNKtonk(hks->buffer, hks->taille_fic, nk_h, hks->position, i);
      if (nk_h_tmp == NULL)continue;

      nbSubKey2 = GetSubNK(hks->buffer, hks->taille_fic, nk_h_tmp, hks->position, 0, NULL, 0);
      for (j=0;j<nbSubKey2 && start_scan;j++)
      {
        if(GetSubNK(hks->buffer, hks->taille_fic, nk_h_tmp, hks->position, j, tmp_key2, MAX_PATH))
        {
          if(strcmp(v_key,tmp_key2) == 0)
          {
              //read VID :
              vendor_id[0] = tmp_key[4];
              vendor_id[1] = tmp_key[5];
              vendor_id[2] = tmp_key[6];
              vendor_id[3] = tmp_key[7];
              vendor_id[4] = 0;

              //read PID
              product_id[0] = tmp_key[13];
              product_id[1] = tmp_key[14];
              product_id[2] = tmp_key[15];
              product_id[3] = tmp_key[16];
              product_id[4] = 0;

              strncpy(pusb,tmp_key,pusb_size);

              //get SymbolicName
              lecteur[0]      = 0;
              symbolicname[0] = 0;
              snprintf(tmp_key3,MAX_PATH,"%s\\%s\\%s\\device parameters",ckey,tmp_key,tmp_key2);
              if(Readnk_Value(hks->buffer, hks->taille_fic, (hks->pos_fhbin)+HBIN_HEADER_SIZE, hks->position, tmp_key3, NULL,"symbolicname", symbolicname, MAX_PATH))
              {
                HBIN_CELL_NK_HEADER *nk_h_tmp2 = GetRegistryNK(hks->buffer, hks->taille_fic, (hks->pos_fhbin)+HBIN_HEADER_SIZE, hks->position, "mounteddevices");
                if (nk_h != NULL)
                {
                  DWORD nbValue = GetValueData(hks->buffer, hks->taille_fic, nk_h_tmp2, (hks->pos_fhbin)+HBIN_HEADER_SIZE,0,NULL,0,NULL,0)
                        ,k;
                  if (nbValue)
                  {
                    for (k=0;k<nbValue;k++)
                    {
                      value_size = MAX_PATH;data_size = MAX_PATH;
                      value[0] = 0;
                      data[0]  = 0;
                      if(GetValueData(hks->buffer, hks->taille_fic, nk_h_tmp2, (hks->pos_fhbin)+HBIN_HEADER_SIZE,
                                      k,value,value_size,data,data_size))
                      {
                        if (value[0] == '\\' && value[3] != '\\')
                        {
                          snprintf(lecteur,lecteur_size,"%S",data);
                          if (!strcmp(lecteur,symbolicname))
                          {
                            strncpy(lecteur,value,lecteur_size);
                            return;
                          }
                        }
                      }
                      lecteur[0] = 0;
                    }
                  }
                }
              }
              return;
          }
        }
      }
    }
  }
}
//------------------------------------------------------------------------------
void EnumUSB_file(HK_F_OPEN *hks, char*ckey, char *_ckey, unsigned int session_id, sqlite3 *db)
{
  //exist or not in the file ?
  HBIN_CELL_NK_HEADER *nk_h = GetRegistryNK(hks->buffer, hks->taille_fic, (hks->pos_fhbin)+HBIN_HEADER_SIZE, hks->position, ckey);
  if (nk_h == NULL)return;

  char key_path[MAX_PATH];
  char lastupdate[DATE_SIZE_MAX],name[MAX_PATH],vendor_id[MAX_PATH],product_id[MAX_PATH],
       description[MAX_PATH],description1[MAX_PATH],description2[MAX_PATH],pusb[MAX_PATH],lecteur[MAX_PATH];

  char tmp_key[MAX_PATH], tmp_key2[MAX_PATH];
  HBIN_CELL_NK_HEADER *nk_h_tmp, *nk_h_tmp2;
  DWORD i,j,nbSubKey2, nbSubKey = GetSubNK(hks->buffer, hks->taille_fic, nk_h, hks->position, 0, NULL, 0);
  for (i=0;i<nbSubKey && start_scan;i++)
  {
    //for each subkey
    if(GetSubNK(hks->buffer, hks->taille_fic, nk_h, hks->position, i, tmp_key, MAX_PATH))
    {
      //get nk of key :)
      nk_h_tmp = GetSubNKtonk(hks->buffer, hks->taille_fic, nk_h, hks->position, i);
      if (nk_h_tmp == NULL)continue;

      nbSubKey2 = GetSubNK(hks->buffer, hks->taille_fic, nk_h_tmp, hks->position, 0, NULL, 0);
      for (j=0;j<nbSubKey2 && start_scan;j++)
      {
        //for each subkey
        if(GetSubNK(hks->buffer, hks->taille_fic, nk_h_tmp, hks->position, j, tmp_key2, MAX_PATH))
        {
          //get nk of key :)
          nk_h_tmp2 = GetSubNKtonk(hks->buffer, hks->taille_fic, nk_h_tmp, hks->position, j);
          if (nk_h_tmp2 == NULL)continue;

          if(Readnk_Value(hks->buffer, hks->taille_fic, (hks->pos_fhbin)+HBIN_HEADER_SIZE, hks->position, NULL, nk_h_tmp2,"Class", description1, MAX_PATH))
          {
            //read datas ^^
            snprintf(key_path,MAX_PATH,"%s\\%s\\%s",ckey,tmp_key,tmp_key2);

            Readnk_Infos(hks->buffer, hks->taille_fic, (hks->pos_fhbin), hks->position, NULL, nk_h_tmp2,
                         lastupdate, DATE_SIZE_MAX, NULL, 0, NULL, 0);

            Readnk_Value(hks->buffer, hks->taille_fic, (hks->pos_fhbin)+HBIN_HEADER_SIZE, hks->position, NULL, nk_h_tmp2,"Mfg", description2, MAX_PATH);
            snprintf(description,MAX_PATH,"%s %s (%s)",description1,description2,tmp_key);

            Readnk_Value(hks->buffer, hks->taille_fic, (hks->pos_fhbin)+HBIN_HEADER_SIZE, hks->position, NULL, nk_h_tmp2,"FriendlyName", name, MAX_PATH);

            SearchVidPid_file(hks, _ckey, tmp_key2,vendor_id,MAX_PATH,product_id,MAX_PATH,pusb,MAX_PATH,lecteur,MAX_PATH);

            convertStringToSQL(description, MAX_PATH);
            addRegistryUSBtoDB(hks->file, "", key_path, name, vendor_id, product_id, description,pusb,lecteur, lastupdate, session_id, db);
          }
        }
      }
    }
  }
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD WINAPI Scan_registry_usb(LPVOID lParam)
{
  //init
  sqlite3 *db = (sqlite3 *)db_scan;
  unsigned int session_id = current_session_id;

  char file[MAX_PATH];
  HK_F_OPEN hks;

  #ifdef CMD_LINE_ONLY_NO_DB
  printf("\"USB\";\"file\";\"hk\";\"key\";\"name\";\"vendor_id\";\"product_id\";\"description\";\"pusb\";\"lecteur\";\"last_use\";\"session_id\";\r\n");
  #endif
  //files or local
  if(!SQLITE_FULL_SPEED)sqlite3_exec(db_scan,"BEGIN TRANSACTION;", NULL, NULL, NULL);
  HTREEITEM hitem = (HTREEITEM)SendMessage(htrv_files, TVM_GETNEXTITEM,(WPARAM)TVGN_CHILD, (LPARAM)TRV_HTREEITEM_CONF[FILES_TITLE_REGISTRY]);
  if (hitem!=NULL || !LOCAL_SCAN) //files
  {
    while(hitem!=NULL && start_scan)
    {
      file[0] = 0;
      GetTextFromTrv(hitem, file, MAX_PATH);
      if (file[0] != 0)
      {

        //open file + verify
        if(OpenRegFiletoMem(&hks, file))
        {
          EnumUSB_file(&hks,"ControlSet001\\Enum\\USBSTOR", "ControlSet001\\Enum\\USB", session_id, db);
          EnumUSB_file(&hks,"ControlSet002\\Enum\\USBSTOR", "ControlSet002\\Enum\\USB", session_id, db);
          EnumUSB_file(&hks,"ControlSet003\\Enum\\USBSTOR", "ControlSet003\\Enum\\USB", session_id, db);
          EnumUSB_file(&hks,"ControlSet004\\Enum\\USBSTOR", "ControlSet004\\Enum\\USB", session_id, db);
          CloseRegFiletoMem(&hks);
        }
      }
      hitem = (HTREEITEM)SendMessage(htrv_files, TVM_GETNEXTITEM,(WPARAM)TVGN_NEXT, (LPARAM)hitem);
    }
  }else
  {
    EnumUSB_local(HKEY_LOCAL_MACHINE, "HKEY_LOCAL_MACHINE", "SYSTEM\\CurrentControlSet\\Enum\\USBSTOR", session_id, db);
  }

  if(!SQLITE_FULL_SPEED)sqlite3_exec(db_scan,"END TRANSACTION;", NULL, NULL, NULL);
  check_treeview(htrv_test, H_tests[(unsigned int)lParam], TRV_STATE_UNCHECK);//db_scan
  h_thread_test[(unsigned int)lParam] = 0;
  return 0;
}
