//------------------------------------------------------------------------------
// Projet RTCA          : READ TO CATCH ALL
// Auteur               : Nicolas Hanteville
// Site                 : http://code.google.com/p/omnia-projetcs/
// Licence              : GPL V3
//------------------------------------------------------------------------------
#include "resource.h"
//------------------------------------------------------------------------------
BOOL AdministratorGroupName(char *group_name, unsigned short gn_max_size)
{
  //loading DLL
  HMODULE hDLL;
  BOOL ret = FALSE;
  typedef NET_API_STATUS (WINAPI *NETAPIBUFFERFREE)(LPVOID Buffer);
  NETAPIBUFFERFREE NetApiBufferFree;

  typedef NET_API_STATUS (WINAPI *NETGROUPENUM)(LPCWSTR servername, DWORD level, LPBYTE* bufptr, DWORD prefmaxlen, LPDWORD entriesread, LPDWORD totalentries, LPDWORD resume_handle);
  NETGROUPENUM NetLocalGroupEnum;

  if ((hDLL = LoadLibrary( "NETAPI32.dll"))!=NULL)
  {
    NetApiBufferFree = (NETAPIBUFFERFREE) GetProcAddress(hDLL,"NetApiBufferFree");
    NetLocalGroupEnum = (NETGROUPENUM) GetProcAddress(hDLL,"NetLocalGroupEnum");

    if (NetApiBufferFree && NetLocalGroupEnum)
    {
      //Enumerate group to find Administrator group
      LPLOCALGROUP_INFO_0 pBuf = 0;
      DWORD nb = 0, total=0;

      NET_API_STATUS nStatus = NetLocalGroupEnum(0,0,(LPBYTE*)&pBuf,2048,&nb,&total,0);
      if (((nStatus == NERR_Success) || (nStatus == ERROR_MORE_DATA)) && (pBuf) != 0 && nb>0)
      {
          //le 1er compte est toujour l'administrateur, ils sont charg�s dans l'ordre de rid!
          sprintf(group_name,"%S",pBuf->lgrpi0_name);
          ret = TRUE;
      }
      NetApiBufferFree(pBuf);
    }
    FreeLibrary(hDLL);
  }

  return ret;
}
//------------------------------------------------------------------------------
//test si nous somme sous l'environnement Wine
BOOL isWine()
{
  HKEY CleTmp=0;
  if (RegOpenKey(HKEY_LOCAL_MACHINE,"Software\\Wine\\",&CleTmp)==ERROR_SUCCESS)
  {
    RegCloseKey(CleTmp);
    return TRUE;
  }
  return FALSE;
}

//------------------------------------------------------------------------------
int LireTreeTxt(HANDLE hparent, DWORD treeview, HTREEITEM hitem, char *txt, DWORD max_size)
{
  txt[0]=0;
  TV_ITEM tvi;
  tvi.mask = TVIF_TEXT;
  tvi.pszText = txt;
  tvi.iImage =0;
  tvi.cchTextMax = max_size;
  tvi.hItem = hitem;
  SendDlgItemMessage(hparent, treeview, TVM_GETITEM, (WPARAM)NULL, (LPARAM)(long)&tvi);
  return strlen(txt);
}
//------------------------------------------------------------------------------
void GetItemPath(HANDLE hparent, DWORD treeview, HTREEITEM hitem, char *path, DWORD max_size)
{
  char tmp_path[MAX_PATH],tmp_path2[MAX_PATH];
  path[0]=0;
  HTREEITEM hitem_parent = hitem;

  do{
    LireTreeTxt(hparent, treeview, hitem_parent,tmp_path, max_size);
    if (path[0]==0)
    {
      strncpy(path,tmp_path,max_size);
    }else
    {
      if (tmp_path[strlen(tmp_path)-1]=='\\')snprintf(tmp_path2,MAX_PATH,"%s%s",tmp_path,path);
      else snprintf(tmp_path2,MAX_PATH,"%s\\%s",tmp_path,path);
      strncpy(path,tmp_path2,max_size);
    }
  }while ((hitem_parent = (HTREEITEM)SendDlgItemMessage(hparent, treeview, TVM_GETNEXTITEM, (WPARAM)TVGN_PARENT, (LPARAM)hitem_parent)));
}
//------------------------------------------------------------------------------
void ErrorExit(LPTSTR lpszFunction)
{
    // Retrieve the system error message for the last-error code
    char* lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    // Display the error message and exit the process
    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
        sprintf(lpDisplayBuf,"%s failed with error %lu: %s",lpszFunction, dw, lpMsgBuf);
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    //ExitProcess(dw);
}
//------------------------------------------------------------------------------
DWORD WINAPI csvImport(LPVOID lParam)
{
  //r�cup�ration du path
  OPENFILENAME ofn;
  char fichier[MAX_PATH]="";
  ZeroMemory(&ofn, sizeof(OPENFILENAME));
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hwndOwner = Tabl[TABL_MAIN];
  ofn.lpstrFile = fichier;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter ="CSV (*.csv)\0*.csv\0";
  ofn.nFilterIndex = 1;
  ofn.Flags =OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_FILEMUSTEXIST| OFN_ALLOWMULTISELECT | OFN_EXPLORER;
  ofn.lpstrDefExt ="*.csv\0";

  if (GetOpenFileName(&ofn)==TRUE)
  {
    if (ofn.nFileOffset > 0)
    {
      //ouverture du fichier
      char *buffer=NULL;
      DWORD t_taille_fic = 0, i=0, j, k;
      HANDLE Hsrc = CreateFile(fichier,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,0);
      if (Hsrc != INVALID_HANDLE_VALUE)
      {
        //on r�cup�re le contenu du fichier dans un buffer
        t_taille_fic = GetFileSize(Hsrc,NULL);
        if (t_taille_fic>0 && t_taille_fic!=INVALID_FILE_SIZE)
        {
          //allocation de la m�moire pour le buffer
           buffer = (char*)LocalAlloc(LMEM_FIXED, sizeof(char)*t_taille_fic+1);
           if (buffer)
           {
             DWORD copiee;
             DWORD place = 0;
             DWORD increm = 0;

             if (t_taille_fic > DIXM)increm = DIXM;
             else increm = t_taille_fic;

             while (place<t_taille_fic && increm!=0)//gestion pour �viter les bug de sync permet une ouverture de fichiers �normes ^^
             {
               copiee = 0;
               ReadFile(Hsrc, buffer+place, increm,&copiee,0);
               place +=copiee;
               if (t_taille_fic-place < increm)increm = t_taille_fic-place ;
             }
           }else SB_add_T(TABL_CONF-1, "CSV LOADING : Out of memory");
        }else SB_add_T(TABL_CONF-1, "CSV LOADING : File size error");
      }else SB_add_T(TABL_CONF-1, "CSV LOADING : Acces file error");
      CloseHandle(Hsrc);

      if (buffer != NULL)
      {
        //listeview
        HANDLE hlv=NULL;
        unsigned int nb_colonne = 0;
        switch(TABL_ID_VISIBLE)
        {
          case TABL_LOGS:     hlv = GetDlgItem(Tabl[TABL_LOGS]    ,LV_LOGS_VIEW);nb_colonne = NB_COLONNE_LV[LV_LOGS_VIEW_NB_COL];break;
          case TABL_FILES:    hlv = GetDlgItem(Tabl[TABL_FILES]   ,LV_FILES_VIEW);nb_colonne = NB_COLONNE_LV[LV_FILES_VIEW_NB_COL];break;
          case TABL_REGISTRY: hlv = GetDlgItem(Tabl[TABL_REGISTRY],LV_REGISTRY_VIEW);nb_colonne = NB_COLONNE_LV[LV_REGISTRY_CONF_NB_COL];break;
        }

        //traitement !
        if (hlv!=NULL)
        {
          //cas d'utlisation pour la base de registre
          BOOL first_line = TRUE;
          if (TABL_ID_VISIBLE == TABL_REGISTRY)
          {
            LINE_ITEM lv_line[SIZE_UTIL_ITEM];
            char tmp[MAX_LINE_SIZE];

            //puis on traite
            for (i=0;i<t_taille_fic;i++)
            {
              //nombre d'�l�ment
              for (j=0;j<nb_colonne;j++)
              {
                k=0;
                while((buffer[i] != '"' || buffer[i+1] != ';') && k<(MAX_LINE_SIZE-1) && i<t_taille_fic)tmp[k++] = buffer[i++];

                if (buffer[i] == '"')
                {
                  if (k>0)
                  {
                    tmp[k]=0;
                    strcpy(lv_line[j].c,tmp+1);
                  }else lv_line[j].c[0]=0;
                  i+=2;
                }
              }
              //ajout de l'item
              if (!first_line)AddToLVRegBin(hlv, lv_line, NB_COLONNE_LV[LV_REGISTRY_CONF_NB_COL]);
              else first_line = FALSE;

              //on passe � la ligne suivante
              while (buffer[i++] != '\r' && i<t_taille_fic)i++;

              //tous les 10 megas ^^
              if(i%DIXM)SB_add(TABL_CONF-1, "CSV LOADING",i,t_taille_fic);
            }
          }else //ajout normal pour les autres ^^
          {
            DWORD itemPosP=0;
            char tmp[MAX_LINE_SIZE],tmp2[MAX_LINE_SIZE];
            LVITEM lvi;
            lvi.mask = LVIF_TEXT|LVIF_PARAM;
            lvi.iSubItem = 0;
            lvi.lParam = LVM_SORTITEMS;
            lvi.pszText="";
            lvi.iItem = ListView_GetItemCount(hlv);

            //puis on traite
            for (i=0;i<t_taille_fic;i++)
            {
              //cr�ation de la ligne
              if (!first_line)
              {
                itemPosP = ListView_InsertItem(hlv, &lvi);
                lvi.iItem = lvi.iItem+1;
              }

              //nombre d'�l�ment
              for (j=0;j<nb_colonne;j++)
              {
                k=0;
                while((buffer[i] != '"' || buffer[i+1] != ';') && k<(MAX_LINE_SIZE-1) && i<t_taille_fic)tmp[k++] = buffer[i++];

                if (buffer[i] == '"')
                {
                  if (k>0)
                  {
                    tmp[k]=0;
                    if (!first_line)
                    {
                      strcpy(tmp2,tmp+1);  //on passe le "
                      //remplissage de la ligne
                      ListView_SetItemText(hlv,itemPosP,j,tmp2);
                    }
                  }
                  i+=2;
                }
              }
              if (first_line)first_line=FALSE;
              //on passe � la ligne suivante
              while (buffer[i++] != '\r' && i<t_taille_fic)i++;

              //tous les 10 megas ^^
              //if(i%DIXM == 1)SB_add(TABL_CONF-1, "CSV LOADING",i,t_taille_fic);
            }
          }
        }else SB_add_T(TABL_CONF-1, "CSV LOADING : LSV error");
        //lib�ration de la m�moire
        LocalFree(buffer);
      }
    }
  }
  return 0;
}

//------------------------------------------------------------------------------
//fonction du lancement de scan global
DWORD WINAPI Scan(LPVOID lParam)
{
  //r�cup�ration des �tats de configuration
  BOOL local    = FALSE;
  syskey_ = FALSE;
  secret_ = FALSE;
  sk_c[0]=0;
  secret_c[0]=0;

  if (IsDlgButtonChecked(Tabl[TABL_CONF],CHK_CONF_LOCAL)==BST_CHECKED)  local     = TRUE;

  if (IsDlgButtonChecked(Tabl[TABL_CONF],CHK_CONF_LOGS)     ==BST_CHECKED)h_scan_logs     = CreateThread(NULL,0,Scan_logs,(LPVOID)local,0,0);
  if (IsDlgButtonChecked(Tabl[TABL_CONF],CHK_CONF_FILES)    ==BST_CHECKED)h_scan_files    = CreateThread(NULL,0,Scan_files,(LPVOID)local,0,0);
  if (IsDlgButtonChecked(Tabl[TABL_CONF],CHK_CONF_REGISTRY) ==BST_CHECKED)h_scan_registry = CreateThread(NULL,0,Scan_registry,(LPVOID)local,0,0);

  //Init de state
  ListView_DeleteAllItems(GetDlgItem(Tabl[TABL_STATE],LV_VIEW));

  //si aucun scan on quitte
  if (!h_scan_logs && !h_scan_files && !h_scan_registry)
  {
    ScanStart = FALSE;
    SetWindowText(GetDlgItem(Tabl[TABL_CONF],BT_CONF_START),"Start");
  }
  return 0;
}
//------------------------------------------------------------------------------
DWORD WINAPI StopScan(LPVOID lParam)
{
  DWORD IDThread;
  if (h_scan_logs != NULL)
  {
    GetExitCodeThread(h_scan_logs,&IDThread);
    TerminateThread(h_scan_logs,IDThread);
    h_scan_logs     = NULL;
    MiseEnGras(Tabl[TABL_MAIN],BT_MAIN_LOGS,FALSE);
  }
  if (h_scan_files != NULL)
  {
    GetExitCodeThread(h_scan_files,&IDThread);
    TerminateThread(h_scan_files,IDThread);
    h_scan_files    = NULL;
    MiseEnGras(Tabl[TABL_MAIN],BT_MAIN_FILES,FALSE);
  }
  if (h_scan_registry != NULL)
  {
    GetExitCodeThread(h_scan_registry,&IDThread);
    TerminateThread(h_scan_registry,IDThread);
    h_scan_registry = NULL;
    MiseEnGras(Tabl[TABL_MAIN],BT_MAIN_REGISTRY,FALSE);
  }

  EnableWindow(GetDlgItem(Tabl[TABL_CONF],BT_CONF_START),TRUE);
  return 0;
}
//------------------------------------------------------------------------------
void CopyData(HANDLE hlv, DWORD ligne, unsigned short colonne)
{
  char tmp[MAX_LINE_SIZE]="";
  ListView_GetItemText(hlv,ligne,colonne,tmp,MAX_LINE_SIZE);

  //copy dans le presse papier
  if(OpenClipboard(NULL))
  {
    EmptyClipboard();
    HANDLE hGlobal = GlobalAlloc(GHND | GMEM_SHARE, strlen(tmp)+1);
    if (hGlobal!=NULL)
    {
      char *p = (char *)GlobalLock(hGlobal);
      if (p != NULL)
      {
        strcpy(p, tmp);
      }
      GlobalUnlock(hGlobal);
      SetClipboardData(CF_TEXT, hGlobal);
    }
    CloseClipboard();
  }
}
//------------------------------------------------------------------------------
void CopyTVData(HANDLE hparent, DWORD treeview, HTREEITEM hitem)
{
  char tmp[MAX_PATH];
  GetItemPath(hparent, treeview, hitem, tmp, MAX_PATH);

  //copy dans le presse papier
  if(OpenClipboard(NULL))
  {
    EmptyClipboard();
    HANDLE hGlobal = GlobalAlloc(GHND | GMEM_SHARE, strlen(tmp)+1);
    if (hGlobal!=NULL)
    {
      char *p = (char *)GlobalLock(hGlobal);
      if (p != NULL)
      {
        strcpy(p, tmp);
      }
      GlobalUnlock(hGlobal);
      SetClipboardData(CF_TEXT, hGlobal);
    }
    CloseClipboard();
  }
}
//------------------------------------------------------------------------------
void SB_add(unsigned short colonne, char *desc, DWORD place, DWORD val_ref)
{
  char tmp[50];
  if (val_ref == 0)snprintf(tmp,49,"%s %lu",desc,place);
  else snprintf(tmp,49,"%s %lu%%",desc,place*100/val_ref);
  SendDlgItemMessage(Tabl[TABL_MAIN],SB_MAIN,SB_SETTEXT,(WPARAM)colonne, (LPARAM)tmp);
}
//------------------------------------------------------------------------------
void SB_add_T(unsigned short colonne, char *desc)
{
  SendDlgItemMessage(Tabl[TABL_MAIN],SB_MAIN,SB_SETTEXT,(WPARAM)colonne, (LPARAM)desc);
}
//------------------------------------------------------------------------------
void MiseEnGras(HWND hwnd,unsigned int bt,BOOL etat)
{
 if (etat)SendDlgItemMessage(hwnd,bt,WM_SETFONT ,(WPARAM)0, (LPARAM)1);
 else SendDlgItemMessage(hwnd,bt,WM_SETFONT ,(WPARAM)PoliceGras, (LPARAM)1);
}
//------------------------------------------------------------------------------
//r�cup�ration des extension entre des ,
DWORD TraitementChaineExt(EXT_TYPE *c, char *buffer,DWORD max_item)
{
  DWORD i=0,j;
  char *p, *b = buffer;
  while (*b && i<max_item)
  {
    j=0;
    p = c[i++].ext;
    while (*b && *b!=',' && j++<NB_MAX_CAR_FILE_EXT)*p++=*b++;
    b++;
    *++p=0;
  }
  return i;
}
//------------------------------------------------------------------------------
BOOL FichierExiste(char *fichierTest)
{
  if (GetFileAttributes(fichierTest) == INVALID_FILE_ATTRIBUTES) return FALSE;
  return TRUE;
};
//------------------------------------------------------------------------------
//emplacement de l'application en cours
void GetMyDirectory(char *path, unsigned int TAILLE)
{
  char *c = path+GetModuleFileName(0, path,TAILLE);
  while(*c != '\\') c--;
  c++;
  *c = 0;
}
//------------------------------------------------------------------------------
void LVAllSearch(HANDLE hlv, unsigned short nb_col, char *search)
{
  DWORD i, j, nb_item = ListView_GetItemCount(hlv);
  char tmp[MAX_LINE_SIZE];

  ListView_SetItemState(hlv, -1, 0, LVIS_SELECTED); // deselect all

  for (i=0;i<nb_item;i++)
  {
    for (j=0;j<nb_col;j++)
    {
      //r�cup�ration du text
      tmp[0]=0;
      ListView_GetItemText(hlv,i,j,tmp,MAX_LINE_SIZE);

      //test si la recherche est pr�sente dedans
      if (Contient(tmp,search))
      {
        //s�lection
        ListView_SetItemState(hlv,i,LVIS_SELECTED,LVIS_SELECTED);
      }
    }
  }
}
//------------------------------------------------------------------------------
DWORD LVSearch(HANDLE hlv, unsigned short nb_col, char *search, DWORD start_id)
{
  DWORD i, j, nb_item = ListView_GetItemCount(hlv);
  char tmp[MAX_LINE_SIZE];
  for (i=start_id+1;i<nb_item;i++)
  {
    for (j=0;j<nb_col;j++)
    {
      //r�cup�ration du text
      tmp[0]=0;
      ListView_GetItemText(hlv,i,j,tmp,MAX_LINE_SIZE);

      //test si la recherche est pr�sente dedans
      if (Contient(tmp,search))
      {
        //s�lection
        SendMessage(hlv, LVM_ENSUREVISIBLE, i, 0);
        ListView_SetItemState(hlv, -1, 0, LVIS_SELECTED); // deselect all
        ListView_SetItemState(hlv,i,LVIS_SELECTED,LVIS_SELECTED);

        //on quitte
        return i;
      }
    }
  }
  return -1;
}
//------------------------------------------------------------------------------
void LVDelete(unsigned int id_tabl, int lv)
{
  //test si des enregistrements
  DWORD NBLigne=SendDlgItemMessage(Tabl[id_tabl],lv,LVM_GETITEMCOUNT,(WPARAM)0,(LPARAM)0);
  if (NBLigne>0)
  {
    int j;
    for (j=NBLigne-1;j>-1;j--)//ligne par ligne
    {
      //on v�rifie que la ligne est bien s�lectionn�e
      if (SendDlgItemMessage(Tabl[id_tabl],lv,LVM_GETITEMSTATE,(WPARAM)j,(LPARAM)LVIS_SELECTED) == LVIS_SELECTED)
        SendDlgItemMessage(Tabl[id_tabl],lv,LVM_DELETEITEM,(WPARAM)j,(LPARAM)NULL);
    }
  }
}
//------------------------------------------------------------------------------
void LVtoMessage(HANDLE hlv, unsigned short colonne)
{
  //r�cup�ration de l'id de l'item si au moin un item
  if (ListView_GetItemCount(hlv))
  {
    int id = SendMessage(hlv,LVM_GETNEXTITEM,-1,LVNI_FOCUSED);
    if (id>-1)
    {
      //lecture de l'item
      char tmp[MAX_LINE_SIZE+1]="";
      ListView_GetItemText(hlv,id,colonne,tmp,MAX_LINE_SIZE);
      if (strlen(tmp)>0)
        MessageBox(0,tmp,"Information",MB_OK|MB_TOPMOST);
    }
  }
}
//------------------------------------------------------------------------------
//HANDLE hListView = GetDlgItem(Tabl[TAB_DISCO],LSTV);
DWORD AddToLV(HANDLE hlv, LINE_ITEM *item, unsigned short nb_colonne)
{
  //ajout de la ligne
  LVITEM lvi;
  lvi.mask = LVIF_TEXT|LVIF_PARAM;
  lvi.iSubItem = 0;
  lvi.lParam = LVM_SORTITEMS;
  lvi.pszText="";
  lvi.iItem = ListView_GetItemCount(hlv);
  DWORD itemPos = ListView_InsertItem(hlv, &lvi);

  //ajout des items
  unsigned short i=0;
  for (;i<nb_colonne;i++){if (item[i].c[0]!=0)ListView_SetItemText(hlv,itemPos,i,item[i].c);}

  return itemPos;
}
//------------------------------------------------------------------------------
unsigned int CharToInt(char c)
{
  switch(c)
  {
    case '0':return 0;
    case '1':return 1;
    case '2':return 2;
    case '3':return 3;
    case '4':return 4;
    case '5':return 5;
    case '6':return 6;
    case '7':return 7;
    case '8':return 8;
    case '9':return 9;
  }
  return 0;
}
//------------------------------------------------------------------------------
void AddToLV_Registry(LINE_ITEM *item)
{
  if (!State_Enable)return;
  if (strlen(item[5].c)>=DATE_SIZE/2)
  {
    EnterCriticalSection(&Sync);
    //recherche de l'emplacement par date ^^
    //si aucun item dans la liste on en ajoute
    HANDLE hlv_r = GetDlgItem(Tabl[TABL_STATE],LV_VIEW);
    DWORD nb_items = ListView_GetItemCount(hlv_r);
    DWORD ref_item =0,i;
    char tmp[MAX_LINE_SIZE];

    //date de cr�ation
    if (nb_items > 0)
    {
      //on boucle tous les items jusqu'a avoir item > au notre
      for (i=0;i<nb_items;i++)
      {
        tmp[0]=0;
        ListView_GetItemText(hlv_r,i,0,tmp,MAX_LINE_SIZE);
        if (strcmp(item[5].c,tmp)<0)
        {
          ref_item = i;
          break;
        }
      }
    }

    //ajout de l'item
    LVITEM lvi;
    lvi.mask = LVIF_TEXT|LVIF_PARAM;
    lvi.iSubItem = 0;
    lvi.lParam = LVM_SORTITEMS;
    lvi.pszText="";
    lvi.iItem = ref_item;
    ListView_InsertItem(hlv_r, &lvi);
    //Date
    ListView_SetItemText(hlv_r,ref_item,0,item[5].c);
    //Source
    ListView_SetItemText(hlv_r,ref_item,1,"Registry");
    //User
    ListView_SetItemText(hlv_r,ref_item,3,"");
    //Description
    snprintf(tmp,MAX_LINE_SIZE,"[Key update] %s\\%s=%s",item[1].c,item[2].c,item[3].c);
    ListView_SetItemText(hlv_r,ref_item,2,tmp);
    LeaveCriticalSection(&Sync);
  }
}
//------------------------------------------------------------------------------
DWORD AddToLV_File(HANDLE hlv, LINE_ITEM *item, unsigned short nb_colonne)
{
  if (!State_Enable)return AddToLV(hlv, item, nb_colonne);

  //recherche de l'emplacement par date ^^
  //si aucun item dans la liste on en ajoute
  HANDLE hlv_r = GetDlgItem(Tabl[TABL_STATE],LV_VIEW);
  DWORD nb_items = ListView_GetItemCount(hlv_r);
  DWORD ref_item =0,i;
  char tmp[MAX_LINE_SIZE];
  LVITEM lvi;

  if (strlen(item[4].c)>=DATE_SIZE/2)
  {
    EnterCriticalSection(&Sync);
    //date de cr�ation
    if (nb_items > 0)
    {
      //on boucle tous les items jusqu'a avoir item > au notre
      for (i=0;i<nb_items;i++)
      {
        tmp[0]=0;
        ListView_GetItemText(hlv_r,i,0,tmp,MAX_LINE_SIZE);
        if (strcmp(item[4].c,tmp)<0)
        {
          ref_item = i;
          break;
        }
      }
    }

    //ajout de l'item
    lvi.mask = LVIF_TEXT|LVIF_PARAM;
    lvi.iSubItem = 0;
    lvi.lParam = LVM_SORTITEMS;
    lvi.pszText="";
    lvi.iItem = ref_item;
    ListView_InsertItem(hlv_r, &lvi);
    //Date
    ListView_SetItemText(hlv_r,ref_item,0,item[4].c);
    //Source
    ListView_SetItemText(hlv_r,ref_item,1,"Files");
    //User
    ListView_SetItemText(hlv_r,ref_item,3,item[7].c);
    //Description
    snprintf(tmp,MAX_LINE_SIZE,"[Create] %s%s",item[0].c,item[1].c);
    ListView_SetItemText(hlv_r,ref_item,2,tmp);
    LeaveCriticalSection(&Sync);
  }

  if (strlen(item[5].c)>=DATE_SIZE/2)
  {
    EnterCriticalSection(&Sync);
    //date de modification
    nb_items = ListView_GetItemCount(hlv_r);
    ref_item =0;
    if (nb_items > 0)
    {
      //on boucle tous les items jusqu'a avoir item > au notre
      for (i=0;i<nb_items;i++)
      {
        tmp[0]=0;
        ListView_GetItemText(hlv_r,i,0,tmp,MAX_LINE_SIZE);
        if (strcmp(item[5].c,tmp)<0)
        {
          ref_item = i;
          break;
        }
      }
    }
    lvi.mask = LVIF_TEXT|LVIF_PARAM;
    lvi.iSubItem = 0;
    lvi.lParam = LVM_SORTITEMS;
    lvi.pszText="";
    lvi.iItem = ref_item;
    ListView_InsertItem(hlv_r, &lvi);
    //Date
    ListView_SetItemText(hlv_r,ref_item,0,item[5].c);
    //Source
    ListView_SetItemText(hlv_r,ref_item,1,"Files");
    //User
    ListView_SetItemText(hlv_r,ref_item,3,item[7].c);
    //Description
    snprintf(tmp,MAX_LINE_SIZE,"[Write] %s%s",item[0].c,item[1].c);
    ListView_SetItemText(hlv_r,ref_item,2,tmp);
    LeaveCriticalSection(&Sync);
  }

  if (strlen(item[6].c)>=DATE_SIZE/2)
  {
    EnterCriticalSection(&Sync);
    //date de derni�re utilisation
    nb_items = ListView_GetItemCount(hlv_r);
    ref_item =0;
    if (nb_items > 0)
    {
      //on boucle tous les items jusqu'a avoir item > au notre
      for (i=0;i<nb_items;i++)
      {
        tmp[0]=0;
        ListView_GetItemText(hlv_r,i,0,tmp,MAX_LINE_SIZE);
        if (strcmp(item[6].c,tmp)<0)
        {
          ref_item = i;
          break;
        }
      }
    }
    lvi.mask = LVIF_TEXT|LVIF_PARAM;
    lvi.iSubItem = 0;
    lvi.lParam = LVM_SORTITEMS;
    lvi.pszText="";
    lvi.iItem = ref_item;
    ListView_InsertItem(hlv_r, &lvi);
    //Date
    ListView_SetItemText(hlv_r,ref_item,0,item[6].c);
    //Source
    ListView_SetItemText(hlv_r,ref_item,1,"Files");
    //User
    ListView_SetItemText(hlv_r,ref_item,3,item[7].c);
    //Description
    snprintf(tmp,MAX_LINE_SIZE,"[Access] %s%s",item[0].c,item[1].c);
    ListView_SetItemText(hlv_r,ref_item,2,tmp);
    LeaveCriticalSection(&Sync);
  }

  //ajout d'item
  return AddToLV(hlv, item, nb_colonne);
}
//------------------------------------------------------------------------------
DWORD AddToLV_log(HANDLE hlv, LINE_ITEM *item, unsigned short nb_colonne)
{
  if (!State_Enable)return AddToLV(hlv, item, nb_colonne);

  if (strlen(item[3].c)>=DATE_SIZE/2)
  {
    //recherche de l'emplacement par date ^^
    //si aucun item dans la liste on en ajoute
    HANDLE hlv_r = GetDlgItem(Tabl[TABL_STATE],LV_VIEW);
    DWORD nb_items = ListView_GetItemCount(hlv_r);
    DWORD ref_item =0;
    char tmp[MAX_LINE_SIZE];

    EnterCriticalSection(&Sync);
    if (nb_items > 0)
    {
      //on boucle tous les items jusqu'a avoir item > au notre
      DWORD i =0;
      for (i=0;i<nb_items;i++)
      {
        tmp[0]=0;
        ListView_GetItemText(hlv_r,i,0,tmp,MAX_LINE_SIZE);
        if (strcmp(item[3].c,tmp)<0)
        {
          ref_item = i;
          break;
        }
      }
    }

    //ajout de l'item
    LVITEM lvi;
    lvi.mask = LVIF_TEXT|LVIF_PARAM;
    lvi.iSubItem = 0;
    lvi.lParam = LVM_SORTITEMS;
    lvi.pszText="";
    lvi.iItem = ref_item;
    ListView_InsertItem(hlv_r, &lvi);
    //Date
    ListView_SetItemText(hlv_r,ref_item,0,item[3].c);
    //Source
    ListView_SetItemText(hlv_r,ref_item,1,"Audit logs");
    //User
    ListView_SetItemText(hlv_r,ref_item,3,item[7].c);
    //Description
    snprintf(tmp,MAX_LINE_SIZE,"[%s] %s : %s %s",item[6].c,item[4].c,item[2].c,item[5].c);
    ListView_SetItemText(hlv_r,ref_item,2,tmp);
    LeaveCriticalSection(&Sync);
  }

  //gestion du temps ^^
  //hlv_r = GetDlgItem(Tabl[TABL_STATE],LV_VIEW_H);


/*
  lvc.cx = 130;       //taille colonne
  lvc.pszText = "Start"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW_H,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 130;       //taille colonne
  lvc.pszText = "End"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW_H,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 140;       //taille colonne
  lvc.pszText = "Duration"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW_H,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 140;       //taille colonne
  lvc.pszText = "User"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW_H,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW_H,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_STATE_H_VIEW_NB_COL] = 4;

*/

  //correlation pour ajout dans l'onglet state
  /*#define DATE_SIZE 20
  #define COL_START 0
  #define COL_END   1
  if (strlen(item[3].c) == DATE_SIZE-1)
  {
    //si une date est pr�sente
    //si aucun item dans la liste on en ajoute
    HANDLE hlv_r = GetDlgItem(Tabl[TABL_STATE],LV_VIEW);
    DWORD nb_items = ListView_GetItemCount(hlv_r);
    if (nb_items == 0)
    {
      //ajout de l'item
      LVITEM lvi;
      lvi.mask = LVIF_TEXT|LVIF_PARAM;
      lvi.iSubItem = 0;
      lvi.lParam = LVM_SORTITEMS;
      lvi.pszText="";
      lvi.iItem = 0;
      ListView_InsertItem(hlv_r, &lvi);

      ListView_SetItemText(hlv_r,0,COL_START,item[3].c);
      ListView_SetItemText(hlv_r,0,COL_END,item[3].c);
      ListView_SetItemText(hlv_r,0,2,"00:00:00");         //dur�e

      ListView_SetItemText(hlv_r,0,3,item[7].c);
    }else
    {
      DWORD i, duree;
      BOOL trouve = FALSE;
      char date_start[DATE_SIZE],date_end[DATE_SIZE],tmp_date[DATE_SIZE];
      for (i=0;i<nb_items && !trouve;i++)
      {
        //On test l'ensemble des items pr�sents afin de v�rifier
        //si il n'est pas plus r�cent ou plus ancien
        //m�me jour ?
        date_start[0]=0;
        ListView_GetItemText(hlv_r,i,COL_START,date_start,DATE_SIZE);
        if (date_start[0]!=0)
        {
          //ann�e + s�parateur + mois + jours
          if (date_start[0] == item[3].c[0] && date_start[1] == item[3].c[1] && date_start[2] == item[3].c[2] && date_start[3] == item[3].c[3] &&
              date_start[4] == item[3].c[4] && date_start[7] == item[3].c[7] && //les s�parateurs ^^
              date_start[5] == item[3].c[5] && date_start[6] == item[3].c[6] && date_start[8] == item[3].c[8] && date_start[9] == item[3].c[9])
          {
            date_end[0]=0;
            ListView_GetItemText(hlv_r,i,COL_END,date_end,DATE_SIZE);

            //m�me jour
            trouve = TRUE;

            //on v�rifie si inf�rieur au start
            if ((date_start[11] > item[3].c[11]) || (date_start[12] > item[3].c[12] && date_start[11] == item[3].c[11]) ||
                (date_start[11] == item[3].c[11] && date_start[12] == item[3].c[12] && date_start[14] > item[3].c[14])  ||
                (date_start[11] == item[3].c[11] && date_start[12] == item[3].c[12] && date_start[14] == item[3].c[14] && date_start[15] > item[3].c[15]) ||
                (date_start[11] == item[3].c[11] && date_start[12] == item[3].c[12] && date_start[14] == item[3].c[14] && date_start[15] == item[3].c[15] && date_start[17] > item[3].c[17]) ||
                (date_start[11] == item[3].c[11] && date_start[12] == item[3].c[12] && date_start[14] == item[3].c[14] && date_start[15] == item[3].c[15] && date_start[17] == item[3].c[17] && date_start[18] > item[3].c[18]))
            {
              ListView_SetItemText(hlv_r,i,COL_START,item[3].c);
              strcpy(date_start,item[3].c);

              ListView_SetItemText(hlv_r,i,3,item[7].c);

              //calcul de  la dur�e
              if (date_start[0]!=0 && date_end[0]!=0)
              {
                    duree = (CharToInt(date_end[11])*36000   + CharToInt(date_end[12])*3600   + CharToInt(date_end[14])*600    + CharToInt(date_end[15])*60   + CharToInt(date_end[17])*10   + CharToInt(date_end[18]))
                          - (CharToInt(date_start[11])*36000 + CharToInt(date_start[12])*3600 + CharToInt(date_start[14])*600  + CharToInt(date_start[15])*60 + CharToInt(date_start[17])*10 + CharToInt(date_start[18]));

                snprintf(tmp_date,DATE_SIZE,"%02lu:%02lu:%02lu",duree/3600,(duree%3600)/60,(duree%3600)%60);
                ListView_SetItemText(hlv_r,i,2,tmp_date);
              }
            }else //ou sup�rieur au end ^^
            {
              if (date_end[0]!=0)
              {
                //on v�rifie si sup�rieur � end
                if ((date_end[11] < item[3].c[11]) || (date_end[12] < item[3].c[12] && date_end[11] == item[3].c[11]) ||
                    (date_end[11] == item[3].c[11] && date_end[12] == item[3].c[12] && date_end[14] < item[3].c[14])  ||
                    (date_end[11] == item[3].c[11] && date_end[12] == item[3].c[12] && date_end[14] == item[3].c[14] && date_end[15] < item[3].c[15]) ||
                    (date_end[11] == item[3].c[11] && date_end[12] == item[3].c[12] && date_end[14] == item[3].c[14] && date_end[15] == item[3].c[15] && date_end[17] < item[3].c[17]) ||
                    (date_end[11] == item[3].c[11] && date_end[12] == item[3].c[12] && date_end[14] == item[3].c[14] && date_end[15] == item[3].c[15] && date_end[17] == item[3].c[17] && date_end[18] < item[3].c[18]))
                {
                  ListView_SetItemText(hlv_r,i,COL_END,item[3].c);
                  strcpy(date_end,item[3].c);

                  ListView_SetItemText(hlv_r,i,3,item[7].c);

                  //calcul de  la dur�e
                  if (date_start[0]!=0 && date_end[0]!=0)
                  {
                    duree = (CharToInt(date_end[11])*36000   + CharToInt(date_end[12])*3600   + CharToInt(date_end[14])*600    + CharToInt(date_end[15])*60   + CharToInt(date_end[17])*10   + CharToInt(date_end[18]))
                          - (CharToInt(date_start[11])*36000 + CharToInt(date_start[12])*3600 + CharToInt(date_start[14])*600  + CharToInt(date_start[15])*60 + CharToInt(date_start[17])*10 + CharToInt(date_start[18]));

                    snprintf(tmp_date,DATE_SIZE,"%02lu:%02lu:%02lu",duree/3600,(duree%3600)/60,(duree%3600)%60);
                    ListView_SetItemText(hlv_r,i,2,tmp_date);
                  }
                }
              }
            }
          }
        }
      }
      //si pas identifi� on ajoute l'item
      if (!trouve)
      {
        //ajout de l'item
        LVITEM lvi;
        lvi.mask = LVIF_TEXT|LVIF_PARAM;
        lvi.iSubItem = 0;
        lvi.lParam = LVM_SORTITEMS;
        lvi.pszText="";
        lvi.iItem = ListView_GetItemCount(hlv_r);
        i = ListView_InsertItem(hlv_r, &lvi);

        ListView_SetItemText(hlv_r,i,COL_START,item[3].c);
        ListView_SetItemText(hlv_r,i,COL_END,item[3].c);

        ListView_SetItemText(hlv_r,i,2,"00:00:00");         //dur�e

        ListView_SetItemText(hlv_r,i,3,item[7].c);
      }
    }
  }*/

  //ajout d'item
  return AddToLV(hlv, item, nb_colonne);
}
//------------------------------------------------------------------------------
void  AddToLVRegBin(HANDLE hlv, LINE_ITEM *item, unsigned short nb_colonne)
{
  //ajout de la ligne
  LVITEM lvi;
  lvi.mask = LVIF_TEXT|LVIF_PARAM;
  lvi.iSubItem = 0;
  lvi.lParam = LVM_SORTITEMS;
  lvi.pszText="";
  lvi.iItem = ListView_GetItemCount(hlv);
  long int itemPos = ListView_InsertItem(hlv, &lvi);

  //ajout des items
  unsigned short i=0;
  for (;i<nb_colonne;i++){if (item[i].c[0]!=0)ListView_SetItemText(hlv,itemPos,i,item[i].c);}

  //gestion du filtre pour ajout aux autres onglets
  FiltreRegData(item);
}
//------------------------------------------------------------------------------
void DeplacerLignes(DWORD ligne1, DWORD ligne2, HANDLE hListView, unsigned short nb_colonne)
{
  char tmp1[MAX_LINE_SIZE];
  char tmp2[MAX_LINE_SIZE];
  unsigned short i=0;

  while (i <nb_colonne)
  {
    ListView_GetItemText(hListView,ligne1,i,tmp1,MAX_LINE_SIZE);
    ListView_GetItemText(hListView,ligne2,i,tmp2,MAX_LINE_SIZE);

    ListView_SetItemText(hListView,ligne1,i,tmp2);
    ListView_SetItemText(hListView,ligne2,i,tmp1);
   i++;
  }
}

//------------------------------------------------------------------------------
/*void sc_Tri(HANDLE hListView, unsigned short colonne_ref, unsigned short nb_colonne)
{
  DWORD i,min=0,max = ListView_GetItemCount(hListView);
  DWORD ref_min, ref_max;
  if (max>0)
  {
    max = max-1;
    char val_min[MAX_LINE_SIZE],
         val_max[MAX_LINE_SIZE],
         tmp[MAX_LINE_SIZE];

    do
    {
      ListView_GetItemText(hListView,min,colonne_ref,val_min,MAX_LINE_SIZE);
      ListView_GetItemText(hListView,max,colonne_ref,val_max,MAX_LINE_SIZE);
      ref_min = min;
      ref_max = max;

      for (i=min+1;i<=max;i++)
      {
        ListView_GetItemText(hListView,i,colonne_ref,tmp,MAX_LINE_SIZE);

        //si plus petit
        if (strcmp(tmp,val_min)<0)
        {
          strcpy(val_min,tmp);
          ref_min = i;
        }else if (strcmp(tmp,val_max)>0)
        {
          strcpy(val_max,tmp);
          ref_max = i;
        }
      }
      if (ref_min!=min)DeplacerLignes(min,ref_min,hListView,nb_colonne);
      if (ref_max!=max)DeplacerLignes(max,ref_max,hListView,nb_colonne);
    }while(++min<=--max);
  }
}*/

//2�me technique plus lente et prend plus de ressource (moins bugu�) !!!
void sc_Tri(HANDLE hListView, unsigned short colonne_ref, unsigned short nb_colonne)
{
  DWORD i,j,max = ListView_GetItemCount(hListView);
  if (max > 0)
  {
    //allocation de la m�moire
    LINE_ITEM *lv_line;
    lv_line = malloc(sizeof(LINE_ITEM)*max);
    if (lv_line == NULL)return;

    //copie dans la zone m�moire
    for (i=0;i<max;i++)
    {
      ListView_GetItemText(hListView,i,colonne_ref,lv_line[i].c,MAX_LINE_SIZE);
    }

    //tri + copie
    DWORD ref_min;
    for (i=0;i<max;i++)
    {
      ref_min = i;
      for (j=i;j<max;j++)
      {
        if (strcmp(lv_line[ref_min].c,lv_line[j].c)>0)
          ref_min = j;
      }

      //copie de l'item ^^
      DeplacerLignes(i,ref_min,hListView,nb_colonne);
      //copie de la zone m�moire pour test
      strcpy(lv_line[ref_min].c,lv_line[i].c);
    }
    free(lv_line);
  }
}

//------------------------------------------------------------------------------
DWORD WINAPI Tc_Tri(LPVOID lParam)
{
  if (!Tc_TriStart)
  {
    Tc_TriStart = TRUE;
    STRI* stri = (STRI*)lParam;
    sc_Tri(stri->hwndFrom, stri->colonne_ref, stri->nb_colonne);
  }
  Tc_TriStart = FALSE;
  return 0;
}
//------------------------------------------------------------------------------
void c_Tri(HANDLE hListView, unsigned short colonne_ref, unsigned short nb_colonne)
{
  if (Tc_TriStart)
  {
    Tc_TriStart = FALSE;
    DWORD IDThread;
    GetExitCodeThread(h_Tc_Tri,&IDThread);
    TerminateThread(h_Tc_Tri,IDThread);
    h_Tc_Tri = NULL;
  }else
  {
    STRI stri;
    stri.hwndFrom = hListView;
    stri.colonne_ref = colonne_ref;
    stri.nb_colonne = nb_colonne;
    h_Tc_Tri = CreateThread(NULL,0,Tc_Tri,(LPVOID)&stri,0,0);
  }
}
//------------------------------------------------------------------------------
void TraiterPopupSave(WPARAM wParam, LPARAM lParam, HWND hwnd, unsigned int nb_col)
{
  //traitement des autres popup (treeview)
  if (TABL_ID_VISIBLE == TABL_FILES && TV_FILES_VISBLE)
  {
    //seulement si au moin un item, car obligatoirement s�lectionn� ^^
    if (SendDlgItemMessage(hwnd,TV_VIEW,TVM_GETCOUNT,(WPARAM)NULL, (LPARAM)NULL)>0)
    {
      HMENU hmenu;
      if ((hmenu = LoadMenu(hInst, MAKEINTRESOURCE(POPUP_TV_F)))!= NULL)
      {
        //affichage du popup menu
        TrackPopupMenuEx(GetSubMenu(hmenu, 0), 0, LOWORD(lParam), HIWORD(lParam), hwnd, NULL);
        DestroyMenu(hmenu);
      }
      return;
    }
  }else if (TABL_ID_VISIBLE == TABL_REGISTRY && TABL_ID_REG_VISIBLE == 0 && TV_REGISTRY_VISIBLE)
  {
    //seulement si au moin un item, car obligatoirement s�lectionn� ^^
    if (SendDlgItemMessage(hwnd,TV_VIEW,TVM_GETCOUNT,(WPARAM)NULL, (LPARAM)NULL)>0)
    {
      HMENU hmenu;
      if ((hmenu = LoadMenu(hInst, MAKEINTRESOURCE(POPUP_TV_R)))!= NULL)
      {
        //affichage du popup menu
        TrackPopupMenuEx(GetSubMenu(hmenu, 0), 0, LOWORD(lParam), HIWORD(lParam), hwnd, NULL);
        DestroyMenu(hmenu);
      }
      return;
    }
  }

  //si au moin un item ^^
  if (ListView_GetItemCount((HANDLE)wParam))
  {
    //affichage du popup menu
    HMENU hmenu;
    if ((hmenu = LoadMenu(hInst, MAKEINTRESOURCE(POPUP_LV)))!= NULL)
    {
      //si aucune s�lection on sauvegarde seulement
      if (ListView_GetSelectedCount((HANDLE)wParam)<1)
      {
        EnableMenuItem(hmenu,POPUP_LV_S_SELECTION,MF_BYCOMMAND|MF_GRAYED);
        EnableMenuItem(hmenu,POPUP_LV_S_DELETE,MF_BYCOMMAND|MF_GRAYED);

        RemoveMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_GRAYED);
        RemoveMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_GRAYED);
        RemoveMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_GRAYED);
        RemoveMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_GRAYED);
        RemoveMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_GRAYED);
        RemoveMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_GRAYED);
        RemoveMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_GRAYED);
        RemoveMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_GRAYED);
        RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
        RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
        RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
        RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
      }else
      {
        switch(nb_col)
        {
          case LV_LOGS_VIEW_NB_COL://8
            //on modifie les titre et on supprime les colonnes en trop
            ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : File");
            ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : Index");
            ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : ID");
            ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : Date");
            ModifyMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : Source");
            ModifyMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL6,"Copy : Description");
            ModifyMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL7,"Copy : Type");
            ModifyMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL8,"Copy : User/SID");

            RemoveMenu(hmenu,POPUP_LV_P_VIEW,MF_BYCOMMAND|MF_GRAYED);
            RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
            RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
            RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
            RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
          break;
          case LV_FILES_VIEW_NB_COL://10
            //on modifie les titre et on supprime les colonnes en trop
            ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : Path");
            ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : File");
            ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : Type");
            ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : Owner");
            ModifyMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : Create time");
            ModifyMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL6,"Copy : Last write time");
            ModifyMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL7,"Copy : Last access Time");
            ModifyMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL8,"Copy : ACL");
            ModifyMenu(hmenu,POPUP_LV_P_VIEW,MF_BYCOMMAND|MF_STRING,POPUP_LV_P_VIEW,"Open path");

            RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
            RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
            RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
            RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
          break;
          case LV_REGISTRY_CONF_NB_COL:
            switch(TABL_ID_REG_VISIBLE)
            {
              case LV_REGISTRY_CONF_NB_COL-LV_FILES_VIEW_NB_COL:
                RemoveMenu(hmenu,POPUP_LV_I_VIEW,MF_BYCOMMAND|MF_GRAYED);
              case 0:
                //on modifie les titre et on supprime les colonnes en trop
                ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : File");
                ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : Key");
                ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : Value");
                ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : Data");
                ModifyMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : Type/Description");
                ModifyMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL6,"Copy : Update");

                RemoveMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_GRAYED);
                RemoveMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_GRAYED);
                RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
                RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
                RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
                RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
              break;
              default :
                RemoveMenu(hmenu,POPUP_LV_I_VIEW,MF_BYCOMMAND|MF_GRAYED);
                switch(TABL_ID_REG_VISIBLE+LV_FILES_VIEW_NB_COL)
                {
                  case LV_REGISTRY_LOGICIEL_NB_COL:
                  //on modifie les titre et on supprime les colonnes en trop
                  ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : File");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : Key");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : Name");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : Publisher");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : Install date");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : UninstallString");

                  RemoveMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
                break;
                case LV_REGISTRY_USERS_NB_COL:
                  //on modifie les titre et on supprime les colonnes en trop
                  ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : File");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : Key");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : Name");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : SID-RID");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : Description");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL6,"Copy : Group");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL7,"Copy : Last logon");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL8,"Copy : Nb Connection");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL9,"Copy : State");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL10,"Copy : Password hash");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL12,"Copy : F Binary data");

                  RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
                break;
                case LV_REGISTRY_MAJ_NB_COL:
                  ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : File");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : Key");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : Component");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : Name");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : Description");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL6,"Copy : Installed date");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL7,"Copy : Installed by");

                  RemoveMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
                break;
                case LV_REGISTRY_SERVICES_NB_COL:
                  ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : File");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : Key");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : Name");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : State");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : Path");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL6,"Copy : Description");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL7,"Copy : Type");

                  RemoveMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
                break;
                case LV_REGISTRY_HISTORIQUE_NB_COL:
                  ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : File");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : Key");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : Path");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : Use count");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : Last use");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL6,"Copy : User");

                  RemoveMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
                break;
                case LV_REGISTRY_USB_NB_COL:
                  ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : File");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : Key");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : Name");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : Description");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : Last use");

                  RemoveMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
                break;
                case LV_REGISTRY_START_NB_COL:
                  ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : File");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : Key");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : Value");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : Data");

                  RemoveMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
                break;
                case LV_REGISTRY_LAN_NB_COL:
                  ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : File");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : Key");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : Card");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : Ip");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : DNS");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL6,"Copy : Gateway");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL7,"Copy : DHCP");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL8,"Copy : Wi-Fi");

                  RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
                break;
                case LV_REGISTRY_PASSWORD_NB_COL:
                  ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : File");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : Key");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : Value");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : Data");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : Password");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL6,"Copy : Description");

                  RemoveMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
                break;
                case LV_REGISTRY_MRU_NB_COL:
                  ModifyMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL1,"Copy : File");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL2,"Copy : Key");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL3,"Copy : Value");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL4,"Copy : Data");
                  ModifyMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_STRING,POPUP_LV_CP_COL5,"Copy : Description");

                  RemoveMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
                break;
                default :
                  RemoveMenu(hmenu,POPUP_LV_CP_COL1,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL2,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL3,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL4,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL5,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL6,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL7,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL8,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL9,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL10,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL11,MF_BYCOMMAND|MF_GRAYED);
                  RemoveMenu(hmenu,POPUP_LV_CP_COL12,MF_BYCOMMAND|MF_GRAYED);
                break;
                }
              break;
            }
          break;
        }
      }

      //affichage du popup menu
      TrackPopupMenuEx(GetSubMenu(hmenu, 0), 0, LOWORD(lParam), HIWORD(lParam), hwnd, NULL);
      DestroyMenu(hmenu);
    }
  }else //seulement le menu import
  {
    if (TABL_ID_VISIBLE == TABL_LOGS || TABL_ID_VISIBLE == TABL_FILES || (TABL_ID_VISIBLE == TABL_REGISTRY && TABL_ID_REG_VISIBLE == 0))
    {
      HMENU hmenu;
      if ((hmenu = LoadMenu(hInst, MAKEINTRESOURCE(POPUP_LV_I)))!= NULL)
      {
        //affichage du popup menu
        TrackPopupMenuEx(GetSubMenu(hmenu, 0), 0, LOWORD(lParam), HIWORD(lParam), hwnd, NULL);
        DestroyMenu(hmenu);
      }
    }
  }
}
//------------------------------------------------------------------------------
BOOL Compare(char*src,char*dst)
{
  char *s=src, *d=dst;
  while (*s==*d && *s){s++;d++;};
  if (*s!=*d)return FALSE;
  return TRUE;
}
//------------------------------------------------------------------------------
unsigned long int Contient(char*data,char*chaine)
{
  unsigned long int i=0;
  char *d = data;
  char *c = chaine;

  if (!*c || !*d) return 0;

  while (*d)
  {
    c = chaine;
    while (*d == *c && *c && *d){d++;c++;i++;}

    if (*c == 0) return i;
    d++;i++;
  }
  return FALSE;
}
//------------------------------------------------------------------------------
void replace_to_char(char *buffer, unsigned long int taille, char a)
{
  unsigned long int i=0;
  for(i=0;i<taille-3;i+=2)
  {
    if (buffer[i]==0x00 && buffer[i+1]==0x00 && buffer[i+2]==0x00&& buffer[i+3]==0x00)break;
    else if (buffer[i]==0x00 && buffer[i+1]==0x00)buffer[i] = a;
  }
}
//------------------------------------------------------------------------------
void ListeFileType(char *path,char*ext,HTREEITEM hparent)
{
  //�num�ration des fichiers compris dans le r�pertoire avec l'extension d�finie
  WIN32_FIND_DATA data;
  char path_tmp[MAX_PATH],path_final[MAX_PATH];

  //mise en forme de la chaine de recherche
  strncpy(path_tmp,path,MAX_PATH);
  path_tmp[strlen(path_tmp)-1]=0; //supression de '*
  strncat(path_tmp,ext,MAX_PATH);
  strncat(path_tmp,"\0",MAX_PATH);

  HANDLE hfic = FindFirstFile(path_tmp, &data);
  if (hfic != INVALID_HANDLE_VALUE)
  {
    do{
        if(data.cFileName[0] != '.') //si bien un path valide
        {
          strncpy(path_final,path,MAX_PATH);
          path_final[strlen(path_final)-3]=0;//supression de *.*
          strncat(path_final,data.cFileName,MAX_PATH);
          strncat(path_final,"\0",MAX_PATH);
          AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,path_final,hparent);
        }
    }while(FindNextFile (hfic,&data));
  }
  FindClose(hfic);
}
//------------------------------------------------------------------------------
void MultiFileTypeSearc(char *path,SEARCH_C*ext,DWORD nb_search,HTREEITEM hparent)
{
  WIN32_FIND_DATA data;
  HANDLE hfic = FindFirstFile(path, &data);
  char Rep [MAX_PATH];
  unsigned short j;

  if (hfic != INVALID_HANDLE_VALUE)
  {
    for (j=0;j<nb_search;j++)ListeFileType(path,ext[j].c,hparent);

    do
    {
      if(data.cFileName[0] != '.') //si bien un path valide
      {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)//si un r�pertoire
        {
           //on g�re les r�pertoires fils
           strcpy(Rep,path);
           Rep[strlen(Rep)-3]=0;//supression de *.*
           strncat(Rep,data.cFileName,MAX_PATH);
           strncat(Rep,"\\*.*\0",MAX_PATH);
           MultiFileTypeSearc(Rep, ext,nb_search,hparent);
        }
      }
    }while(FindNextFile (hfic,&data)); //r�cup�ration des fichiers 1 par 1
  }
  FindClose(hfic);
}
//------------------------------------------------------------------------------
void MultiFileSearc(char *path, SEARCH_C*fic,DWORD nb_search)
{
  WIN32_FIND_DATA data;
  HANDLE hfic = FindFirstFile(path, &data);
  char Rep [MAX_PATH];
  char path_final[MAX_PATH];
  DWORD i;

  if (hfic != INVALID_HANDLE_VALUE)
  {
    do
    {
      if(data.cFileName[0] != '.') //si bien un path valide
      {
        //mise en forme du chemin pour traitement
        strcpy(Rep,path);
        Rep[strlen(path)-3]=0;

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)//si un r�pertoire
        {
           //on g�re les r�pertoires fils
           strncat(Rep,data.cFileName,MAX_PATH);
           strncat(Rep,"\\*.*\0",MAX_PATH);
           MultiFileSearc(Rep, fic,nb_search);
        }else // un fichier
        {
          for (i=0;i<nb_search;i++)
          {
            if (Compare(fic[i].c,data.cFileName))
            {
              strcpy(path_final,Rep);
              strncat(path_final,data.cFileName,MAX_PATH);
              strncat(path_final,"\0",MAX_PATH);
              AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,path_final,TRV_HTREEITEM[TRV_REGISTRY]);
              break;
            }
          }
        }
      }
    }while(FindNextFile (hfic,&data)); //r�cup�ration des fichiers 1 par 1
  }
  FindClose(hfic);
}
//------------------------------------------------------------------------------
void SupDoublon(HANDLE hf,DWORD trv,HTREEITEM htreeParent)
{
  HTREEITEM hitem = (HTREEITEM)SendDlgItemMessage(hf,trv, TVM_GETNEXTITEM,(WPARAM)TVGN_CHILD, (LPARAM)htreeParent);
  HTREEITEM hprecitem = hitem;
  //on �num�re tous les items fils du treeview
  char tmp[MAX_PATH],prec[MAX_PATH]="";
  do
  {
    //r�cup�ration du texte de l'item
    TVITEM tvitem;
    tmp[0]=0;
    tvitem.hItem = hitem;
    tvitem.mask = TVIF_TEXT;
    tvitem.pszText = tmp;
    tvitem.cchTextMax = MAX_PATH;
    if (SendDlgItemMessage(hf,trv, TVM_GETITEM,(WPARAM)0, (LPARAM)&tvitem))
    {
      if (!strcmp(tmp,prec))
      {
        //on supprime l'item
        SendDlgItemMessage(hf,trv, TVM_DELETEITEM,(WPARAM)NULL, (LPARAM)hitem);
        hitem = hprecitem;
      }else
      {
        strcpy(prec,tmp);
        hprecitem = hitem;
      }
    }
  }while((hitem = (HTREEITEM)SendDlgItemMessage(hf,trv, TVM_GETNEXTITEM,(WPARAM)TVGN_NEXT, (LPARAM)hitem)));
}
//------------------------------------------------------------------------------
DWORD  WINAPI AutoSearchFiles(LPVOID lParam)
{
  //liste des lecteurs
  //recherche des diff�rents lecteurs et ajout � la liste si un disque
  char tmp[MAX_PATH], tmp_path[MAX_PATH];
  int i,j=0,k=0,l=0,nblecteurs = GetLogicalDriveStrings(MAX_PATH,tmp);

  if (nblecteurs>0)
  {
    SEARCH_C files[25];
    strcpy(files[j++].c,"SAM");
    strcpy(files[j++].c,"sam");
    strcpy(files[j++].c,"software");
    strcpy(files[j++].c,"SOFTWARE");
    strcpy(files[j++].c,"software.sav");
    strcpy(files[j++].c,"SOFTWARE.SAV");
    strcpy(files[j++].c,"system");
    strcpy(files[j++].c,"SYSTEM");
    strcpy(files[j++].c,"system.dat");
    strcpy(files[j++].c,"SYSTEM.DAT");
    strcpy(files[j++].c,"system.sav");
    strcpy(files[j++].c,"SYSTEM.SAV");
    strcpy(files[j++].c,"default");
    strcpy(files[j++].c,"DEFAULT");
    strcpy(files[j++].c,"default.sav");
    strcpy(files[j++].c,"DEFAULT.SAV");
    strcpy(files[j++].c,"ntuser.dat");
    strcpy(files[j++].c,"NTUSER.DAT");
    strcpy(files[j++].c,"user.dat");
    strcpy(files[j++].c,"USER.DAT");
    strcpy(files[j++].c,"userdiff");
    strcpy(files[j++].c,"USERDIFF");
    strcpy(files[j++].c,"Reg.dat");
    strcpy(files[j++].c,"Usrclass.dat");
    strcpy(files[j++].c,"Classes.dat");

    SEARCH_C exts_reg[2];
    strcpy(exts_reg[l++].c,"reg");
    strcpy(exts_reg[l++].c,"REG");

    SEARCH_C exts[6];
    strcpy(exts[k++].c,"evt");
    strcpy(exts[k++].c,"EVT");
    strcpy(exts[k++].c,"evtx");
    strcpy(exts[k++].c,"EVTX");
    strcpy(exts[k++].c,"log");
    strcpy(exts[k++].c,"LOG");

    //init du treeview
    SendDlgItemMessage(Tabl[TABL_CONF],TRV_CONF_TESTS,TVM_DELETEITEM,(WPARAM)0, (LPARAM)TVI_ROOT);
    TRV_HTREEITEM[TRV_LOGS] = AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,"Audit logs",TVI_ROOT);
    TRV_HTREEITEM[TRV_FILES] = AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,"Files path",TVI_ROOT);
    TRV_HTREEITEM[TRV_REGISTRY] = AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,"Registry files",TVI_ROOT);

    //recherche
    for (i=0;i<nblecteurs;i+=4)
    {
      switch(GetDriveType(&tmp[i]))
      {
        case DRIVE_FIXED :
        case DRIVE_REMOTE :
        case DRIVE_RAMDISK :
        case DRIVE_REMOVABLE :
          //ajout aux directory
          AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,&tmp[i],TRV_HTREEITEM[TRV_FILES]);

          //traitement de recherche
          strcpy(tmp_path,"_:\\*.*\0");
          tmp_path[0]=tmp[i];

          //fichiers de logs
          MultiFileTypeSearc(tmp_path,exts,k,TRV_HTREEITEM[TRV_LOGS]);
          MultiFileTypeSearc(tmp_path,exts_reg,l,TRV_HTREEITEM[TRV_REGISTRY]);
          //fichiers de base de registre
          MultiFileSearc(tmp_path,files,j);
        break;
      }
    }
    //tri
    SendDlgItemMessage(Tabl[TABL_CONF],TRV_CONF_TESTS,TVM_SORTCHILDREN, TRUE,(LPARAM)TRV_HTREEITEM[TRV_FILES]);
    SendDlgItemMessage(Tabl[TABL_CONF],TRV_CONF_TESTS,TVM_SORTCHILDREN, TRUE,(LPARAM)TRV_HTREEITEM[TRV_LOGS]);
    SendDlgItemMessage(Tabl[TABL_CONF],TRV_CONF_TESTS,TVM_SORTCHILDREN, TRUE,(LPARAM)TRV_HTREEITEM[TRV_REGISTRY]);

    SupDoublon(Tabl[TABL_CONF],TRV_CONF_TESTS,TRV_HTREEITEM[TRV_FILES]);
    SupDoublon(Tabl[TABL_CONF],TRV_CONF_TESTS,TRV_HTREEITEM[TRV_LOGS]);
    SupDoublon(Tabl[TABL_CONF],TRV_CONF_TESTS,TRV_HTREEITEM[TRV_REGISTRY]);

    //on affihe les r�sultats
    SendDlgItemMessage(Tabl[TABL_CONF],TRV_CONF_TESTS,TVM_EXPAND, TVE_EXPAND,(LPARAM)TRV_HTREEITEM[TRV_FILES]);
    SendDlgItemMessage(Tabl[TABL_CONF],TRV_CONF_TESTS,TVM_EXPAND, TVE_EXPAND,(LPARAM)TRV_HTREEITEM[TRV_LOGS]);
    SendDlgItemMessage(Tabl[TABL_CONF],TRV_CONF_TESTS,TVM_EXPAND, TVE_EXPAND,(LPARAM)TRV_HTREEITEM[TRV_REGISTRY]);
  }

  AutoSearchFilesStart = FALSE;
  return 0;
}
//------------------------------------------------------------------------------
void FileToTreeView(char *c_path)
{
  unsigned short i, size = strlen(c_path);
  if (size >3)
  {
   if (c_path[size-4] == '.')
   {
     //si un fichier de base de registre
     if (((c_path[size-3] == 'd' || c_path[size-3] == 'D') && (c_path[size-2] == 'a' || c_path[size-2] == 'A') && (c_path[size-1] == 't' || c_path[size-1] == 'T')) ||
         ((c_path[size-3] == 's' || c_path[size-3] == 'S') && (c_path[size-2] == 'a' || c_path[size-2] == 'A') && (c_path[size-1] == 'v' || c_path[size-1] == 'V')) ||
         ((c_path[size-3] == 'r' || c_path[size-3] == 'R') && (c_path[size-2] == 'e' || c_path[size-2] == 'E') && (c_path[size-1] == 'g' || c_path[size-1] == 'G')))
      {
        AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,c_path,TRV_HTREEITEM[TRV_REGISTRY]);
        CheckDlgButton(Tabl[TABL_CONF],CHK_CONF_REGISTRY,BST_CHECKED);
    //sinon des journaux d'audit
     }else if (((c_path[size-3] == 'l' || c_path[size-3] == 'L') && (c_path[size-2] == 'o' || c_path[size-2] == 'O') && (c_path[size-1] == 'g' || c_path[size-1] == 'G')) ||
         ((c_path[size-3] == 'e' || c_path[size-3] == 'E') && (c_path[size-2] == 'v' || c_path[size-2] == 'V') && (c_path[size-1] == 't' || c_path[size-1] == 'T')))
      {
        AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,c_path,TRV_HTREEITEM[TRV_LOGS]);
        CheckDlgButton(Tabl[TABL_CONF],CHK_CONF_LOGS,BST_CHECKED);
      }
     return;
   }else if (size > 5)
   {
     //LOG
     if (c_path[size-5] == '.' && ((c_path[size-4] == 'e' || c_path[size-4] == 'E') &&(c_path[size-3] == 'v' || c_path[size-3] == 'V') && (c_path[size-2] == 't' || c_path[size-2] == 'T') && (c_path[size-1] == 'x' || c_path[size-1] == 'X')))
     {
       AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,c_path,TRV_HTREEITEM[TRV_LOGS]);
       CheckDlgButton(Tabl[TABL_CONF],CHK_CONF_LOGS,BST_CHECKED);
       return;
     }else if ((c_path[size-3] == '.') && (c_path[size-2] == 'h' || c_path[size-2] == 'H') && (c_path[size-1] == 'v' || c_path[size-1] == 'V'))
     {
       //registry WIN_CE !!!
        AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,c_path,TRV_HTREEITEM[TRV_REGISTRY]);
        CheckDlgButton(Tabl[TABL_CONF],CHK_CONF_REGISTRY,BST_CHECKED);
     }
   }

   //on v�rifier le type
   DWORD t = GetFileAttributes(c_path);
   if ((t&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)//r�pertoire
   {
    strncat(c_path,"\\\0",MAX_PATH);
    AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,c_path,TRV_HTREEITEM[TRV_FILES]);
    CheckDlgButton(Tabl[TABL_CONF],CHK_CONF_FILES,BST_CHECKED);
   }else
   {
     for (i=0;i<size;i++)
     {
       if (c_path[i]=='.')return;
     }
     //BASE DE REGISTRE sans extension
     AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,c_path,TRV_HTREEITEM[TRV_REGISTRY]);
     CheckDlgButton(Tabl[TABL_CONF],CHK_CONF_REGISTRY,BST_CHECKED);
   }
  }else if (c_path[1]==':' && c_path[2]=='\\' && c_path[3]==0)//lecteur
   AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,c_path,TRV_HTREEITEM[TRV_FILES]);
}
//------------------------------------------------------------------------------
void AddFile()
{
  OPENFILENAME ofn;
  char tmp[MAX_PATH]="";
  ZeroMemory(&ofn, sizeof(OPENFILENAME));
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hwndOwner = Tabl[TABL_MAIN];
  ofn.lpstrFile = tmp;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter ="All files (*.*)\0*.*\0Audit (*.evt)\0*.evt\0Audit (*.evtx)\0*.evtx\0Audit (*.log)\0*.log\0Registry (*.reg)\0*.reg\0Registry (*.dat)\0*.dat\0Registry (*.sav)\0*.sav\0Registry (*)\0*.hv\0Windows CE registry (*.hv)\0*\0Directory (*)\0*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags =OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_FILEMUSTEXIST| OFN_ALLOWMULTISELECT | OFN_EXPLORER;
  ofn.lpstrDefExt ="*.*\0";

  if (GetOpenFileName(&ofn)==TRUE)
  {
    if (ofn.nFileOffset > 0)
    {
      //on test si un seul ou plusieurs
      if (tmp[strlen(tmp)+1]==0) //si un seul 2 0 apr�s
      {
        FileToTreeView(tmp);
      }else
      {
        //le premier est le r�pertoire
        char path[MAX_PATH], c_path[MAX_PATH];
        char *c = tmp;
        strcpy(path,tmp); //path de r�f�rence
        while(*c)c++;c++; //on path le path

        //gestion des items :
        do
        {
          snprintf(c_path,MAX_PATH,"%s\\%s",path,c);
          FileToTreeView(c_path);
          while(*c)c++;
          c++; //on passe le 0
        }while (*c);//si 2 zero on arr�te :p
      }
      //on affiche les r�sultats
      SendDlgItemMessage(Tabl[TABL_CONF],TRV_CONF_TESTS,TVM_EXPAND, TVE_EXPAND,(LPARAM)TRV_HTREEITEM[TRV_FILES]);
      SendDlgItemMessage(Tabl[TABL_CONF],TRV_CONF_TESTS,TVM_EXPAND, TVE_EXPAND,(LPARAM)TRV_HTREEITEM[TRV_LOGS]);
      SendDlgItemMessage(Tabl[TABL_CONF],TRV_CONF_TESTS,TVM_EXPAND, TVE_EXPAND,(LPARAM)TRV_HTREEITEM[TRV_REGISTRY]);
      SendDlgItemMessage(Tabl[TABL_CONF],TRV_CONF_TESTS, TVM_SORTCHILDREN,(WPARAM)TRUE, (LPARAM)TVI_ROOT);
    }
  }
}
//------------------------------------------------------------------------------
void AddRep()
{
  BROWSEINFO browser;
  LPITEMIDLIST lip;
  char tmp[MAX_PATH+1]="";
  browser.hwndOwner = Tabl[TABL_MAIN];;
  browser.pidlRoot = 0;
  browser.lpfn = 0;
  browser.iImage = 0;
  browser.ulFlags = BIF_NEWDIALOGSTYLE; //permet la cr�ation d'un dossier
  browser.lParam = 0;
  browser.pszDisplayName = tmp;  //r�sultat ici
  browser.lpszTitle = "Add Directory :";
  lip = SHBrowseForFolder(&browser);
  if (lip != NULL)
  {
    SHGetPathFromIDList(lip,tmp);
    DWORD s = strlen(tmp);
    if (s>0)
    {
      if (tmp[s-1]!='\\')
      {
        tmp[s]='\\';
        tmp[s+1]=0;
      }
      if (tmp[0] !=0)
      {
          AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS, tmp, TRV_HTREEITEM[TRV_FILES]);
          CheckDlgButton(Tabl[TABL_CONF],CHK_CONF_FILES,BST_CHECKED);
      }
      SendDlgItemMessage(Tabl[TABL_CONF],TRV_CONF_TESTS,TVM_EXPAND, TVE_EXPAND,(LPARAM)TRV_HTREEITEM[TRV_FILES]);
    }
  }
}
//------------------------------------------------------------------------------
void GetItemTxt(HTREEITEM hitem,HANDLE hdlg, int treeview,char *txt, unsigned int size)
{
  TV_ITEM tvi;
  txt[0]=0;
  tvi.mask = TVIF_TEXT;
  tvi.pszText = txt;
  tvi.cchTextMax = size;
  tvi.hItem = hitem;
  SendDlgItemMessage(hdlg,treeview,TVM_GETITEM, TVGN_CARET, (long)&tvi);
}
//------------------------------------------------------------------------------
HTREEITEM AjouterItemTreeView(HANDLE hdlg, int treeview, char *texte, HTREEITEM hparent)
{
  TV_INSERTSTRUCT tvitem;
  tvitem.hParent = hparent;
  tvitem.hInsertAfter = TVI_ROOT;
  tvitem.item.mask = TVIF_TEXT;
  tvitem.item.pszText = texte;

  return (HTREEITEM)SendDlgItemMessage(hdlg,treeview, TVM_INSERTITEM,(WPARAM)0, (LPARAM)&tvitem);
}
//------------------------------------------------------------------------------
HTREEITEM AjouterItemTreeViewRep(HANDLE hdlg, int treeview, char *texte, HTREEITEM hparent)
{
  TV_INSERTSTRUCT tvitem;
  tvitem.hParent = hparent;
  tvitem.hInsertAfter = TVI_ROOT;
  tvitem.item.mask = TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE;
  tvitem.item.pszText = texte;
  tvitem.item.iImage=tvitem.item.iSelectedImage=0;

  return (HTREEITEM)SendDlgItemMessage(hdlg,treeview, TVM_INSERTITEM,(WPARAM)0, (LPARAM)&tvitem);
}
//------------------------------------------------------------------------------
HTREEITEM AjouterItemTreeViewFile(HANDLE hdlg, int treeview, char *texte, HTREEITEM hparent,DWORD img_id)
{
  TV_INSERTSTRUCT tvitem;
  tvitem.hParent = hparent;
  tvitem.hInsertAfter = TVI_ROOT;
  tvitem.item.mask = TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE;
  tvitem.item.pszText = texte;
  tvitem.item.iImage=tvitem.item.iSelectedImage=img_id;
  return (HTREEITEM)SendDlgItemMessage(hdlg,treeview, TVM_INSERTITEM,(WPARAM)0, (LPARAM)&tvitem);
}
//------------------------------------------------------------------------------
void DessinerBouton(LPDRAWITEMSTRUCT lpdis)
{
  //v�rification si bien un bouton ^^
  if (lpdis->CtlType == ODT_BUTTON)
  {
    char tmp[TAILLE_TXT_BT];

    //r�cup�ration du texte du bouton
    GetWindowText(lpdis->hwndItem,tmp,TAILLE_TXT_BT);

    //taille du texte
    SIZE dims;
    GetTextExtentPoint32(lpdis->hDC,tmp, strlen(tmp), &dims);

    //couleur du texte
    SetTextColor(lpdis->hDC, GetSysColor(COLOR_BTNTEXT));

    //couleur du bouton si c'est le bouton visible ^^
    if (GetDlgItem(Tabl[TABL_MAIN],TABL_ID_VISIBLE+DLG_MAIN) == lpdis->hwndItem)
     SetBkColor(lpdis->hDC, RGB(255,153,0));//orange
    else
     SetBkColor(lpdis->hDC, GetSysColor(COLOR_BTNFACE));//gris

    //suivant l'�tat du bouton
    BOOL etat=lpdis->itemState & ODS_SELECTED;

    //Ecrire le texte sur le bouton:
    ExtTextOut(lpdis->hDC,(lpdis->rcItem.right-lpdis->rcItem.left-dims.cx)/2+etat, (lpdis->rcItem.bottom-lpdis->rcItem.top-dims.cy)/2+etat, ETO_OPAQUE | ETO_CLIPPED, &lpdis->rcItem,tmp, strlen(tmp), NULL);

    //Dessiner le cadre du bouton selon son �tat:
    DrawEdge(lpdis->hDC, &lpdis->rcItem,(etat ? EDGE_SUNKEN : EDGE_RAISED ), BF_RECT|BF_FLAT | BF_SOFT);
  }
}
//------------------------------------------------------------------------------
void ViewTabl(DWORD tabl_id)
{
  ShowWindow(Tabl[TABL_ID_VISIBLE], SW_HIDE);
  InvalidateRect(GetDlgItem(Tabl[TABL_MAIN],TABL_ID_VISIBLE+DLG_MAIN),NULL, FALSE);
  TABL_ID_VISIBLE = tabl_id;
  ShowWindow(Tabl[TABL_ID_VISIBLE], SW_SHOW);
  InvalidateRect(GetDlgItem(Tabl[TABL_MAIN],TABL_ID_VISIBLE+DLG_MAIN),NULL, FALSE);
}
//------------------------------------------------------------------------------
void InitConfig(HWND hwnd)
{
  //pour la gestion des composants (listeview)
  InitCommonControls();
  InitializeCriticalSection(&Sync);

  //init des variables globales
  Tabl[TABL_MAIN] = hwnd;
  AutoSearchFilesStart = FALSE;
  h_AutoSearchFiles = NULL;
  State_Enable = FALSE;
  MD5_Enable = FALSE;

  ExportStart = FALSE;
  h_Export = NULL;

  ScanStart = FALSE;
  h_scan_logs = NULL;
  h_scan_files = NULL;
  h_scan_registry = NULL;

  Tc_TriStart = FALSE;
  h_Tc_Tri = NULL;

  pos_search_logs = -1;
  pos_search_files = -1;
  pos_search_registry = -1;
  TV_REGISTRY_VISIBLE = FALSE;
  TV_FILES_VISBLE = FALSE;

  //par d�faut le bouton state est masqu�
  ShowWindow(GetDlgItem(Tabl[TABL_MAIN],BT_MAIN_STATE), SW_HIDE);

  PoliceGras = (HFONT)SendMessage(hwnd,WM_GETFONT,(WPARAM)0,(LPARAM)1);

  //icon sur sur la fen�tre
  SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(ICON_APP)));

  //chargement des diff�rents onglets
  Tabl[TABL_CONF]     = CreateDialog(0, MAKEINTRESOURCE(DLG_CONF)     ,Tabl[TABL_MAIN],DialogProc_conf);
  Tabl[TABL_LOGS]     = CreateDialog(0, MAKEINTRESOURCE(DLG_LOGS)     ,Tabl[TABL_MAIN],DialogProc_logs);
  Tabl[TABL_FILES]    = CreateDialog(0, MAKEINTRESOURCE(DLG_FILES)    ,Tabl[TABL_MAIN],DialogProc_files);
  Tabl[TABL_REGISTRY] = CreateDialog(0, MAKEINTRESOURCE(DLG_REGISTRY) ,Tabl[TABL_MAIN],DialogProc_registry);
  Tabl[TABL_PROCESS]  = CreateDialog(0, MAKEINTRESOURCE(DLG_PROCESS)  ,Tabl[TABL_MAIN],DialogProc_process);
  Tabl[TABL_STATE]    = CreateDialog(0, MAKEINTRESOURCE(DLG_STATE)    ,Tabl[TABL_MAIN],DialogProc_state);

  //fen�tre Info ^^
  Tabl[TABL_INFO]     = CreateDialog(0, MAKEINTRESOURCE(DLG_INFO)    ,Tabl[TABL_MAIN],DialogProc_info);

  ShowWindow(Tabl[TABL_LOGS], SW_HIDE);
  ShowWindow(Tabl[TABL_FILES], SW_HIDE);
  ShowWindow(Tabl[TABL_REGISTRY], SW_HIDE);
  ShowWindow(Tabl[TABL_PROCESS], SW_HIDE);
  ShowWindow(Tabl[TABL_STATE], SW_HIDE);
  ShowWindow(Tabl[TABL_CONF], SW_SHOW);
  TABL_ID_VISIBLE = TABL_CONF;

  //test si nous sommes sur un environnement �mul� (wine)
  if (isWine())
  {
    //on masque le bouton process qui n'est pas compatible wine !!
    ShowWindow(GetDlgItem(Tabl[TABL_MAIN],BT_MAIN_PROCESS) ,SW_HIDE);
  }

  //chargement de la liste d'image pour les treeview
  HIMAGELIST hImageList=ImageList_Create(16,16,ILC_COLORDDB | ILC_MASK,6,0);
  if (hImageList != NULL)
  {
    HCURSOR Hicon = LoadIcon(hInst,(LPCTSTR)ICON_DOSSIER);
    ImageList_AddIcon(hImageList,Hicon);
    DeleteObject(Hicon);
    Hicon = LoadIcon(hInst,(LPCTSTR)ICON_FICHIER_BIN);
    ImageList_AddIcon(hImageList,Hicon);
    DeleteObject(Hicon);
    Hicon = LoadIcon(hInst,(LPCTSTR)ICON_FICHIER_DWORD);
    ImageList_AddIcon(hImageList,Hicon);
    DeleteObject(Hicon);
    Hicon = LoadIcon(hInst,(LPCTSTR)ICON_FICHIER_TXT);
    ImageList_AddIcon(hImageList,Hicon);
    DeleteObject(Hicon);
    Hicon = LoadIcon(hInst,(LPCTSTR)ICON_FICHIER_UNKNOW);
    ImageList_AddIcon(hImageList,Hicon);
    DeleteObject(Hicon);
    Hicon = LoadIcon(hInst,(LPCTSTR)ICON_FICHIER);
    ImageList_AddIcon(hImageList,Hicon);
    DeleteObject(Hicon);
    TreeView_SetImageList(GetDlgItem(Tabl[TABL_REGISTRY],TV_VIEW),hImageList,TVSIL_NORMAL);
    TreeView_SetImageList(GetDlgItem(Tabl[TABL_FILES],TV_VIEW),hImageList,TVSIL_NORMAL);
  }

  //initialisation des combobox
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"All");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"Configuration");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"Software");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"Update/Packages");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"Service/Driver");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"UserAssist");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"USB");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"Start");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"Network");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"Users & groups");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"Accounts & passwords");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"MRU & MUICache & history");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_INSERTSTRING,(WPARAM)-1, (LPARAM)"All Path (only offline)");
  SendDlgItemMessage(Tabl[TABL_REGISTRY],CB_REGISTRY_VIEW, CB_SETCURSEL,(WPARAM)0, (LPARAM)0);

  //ent�te des listview
  LVCOLUMN lvc;
  lvc.mask = LVCF_TEXT|LVCF_WIDTH|LVCF_FMT;
  lvc.fmt = LVCFMT_LEFT;
  lvc.cx = 40;       //taille colonne
  lvc.pszText = "File/Event"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_LOGS],LV_LOGS_VIEW,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.pszText = "Index"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_LOGS],LV_LOGS_VIEW,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.pszText = "ID"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_LOGS],LV_LOGS_VIEW,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Date"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_LOGS],LV_LOGS_VIEW,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 70;       //taille colonne
  lvc.pszText = "Source"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_LOGS],LV_LOGS_VIEW,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);
  lvc.cx = 100;     //taille colonne
  lvc.pszText = "Description"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_LOGS],LV_LOGS_VIEW,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  lvc.cx = 50;       //taille colonne
  lvc.pszText = "Type"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_LOGS],LV_LOGS_VIEW,LVM_INSERTCOLUMN,(WPARAM)6, (LPARAM)&lvc);
  lvc.cx = 90;       //taille colonne
  lvc.pszText = "User-SID"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_LOGS],LV_LOGS_VIEW,LVM_INSERTCOLUMN,(WPARAM)7, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_LOGS],LV_LOGS_VIEW,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_LOGS_VIEW_NB_COL] = 8;

  lvc.cx = 50;       //taille colonne
  lvc.pszText = "Path"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_FILES],LV_FILES_VIEW,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = "File"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_FILES],LV_FILES_VIEW,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = "Type"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_FILES],LV_FILES_VIEW,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = "Owner"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_FILES],LV_FILES_VIEW,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 33;       //taille colonne
  lvc.pszText = "Create time"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_FILES],LV_FILES_VIEW,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);
  lvc.pszText = "Last write time"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_FILES],LV_FILES_VIEW,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  lvc.pszText = "Last access Time"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_FILES],LV_FILES_VIEW,LVM_INSERTCOLUMN,(WPARAM)6, (LPARAM)&lvc);
  lvc.cx = 101;       //taille colonne
  lvc.pszText = "ACL"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_FILES],LV_FILES_VIEW,LVM_INSERTCOLUMN,(WPARAM)7, (LPARAM)&lvc);
  lvc.cx = 20;       //taille colonne
  lvc.pszText = "Hidden"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_FILES],LV_FILES_VIEW,LVM_INSERTCOLUMN,(WPARAM)8, (LPARAM)&lvc);
  lvc.cx = 20;       //taille colonne
  lvc.pszText = "System"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_FILES],LV_FILES_VIEW,LVM_INSERTCOLUMN,(WPARAM)9, (LPARAM)&lvc);
  lvc.cx = 20;       //taille colonne
  lvc.pszText = "MD5"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_FILES],LV_FILES_VIEW,LVM_INSERTCOLUMN,(WPARAM)10, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_FILES],LV_FILES_VIEW,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_FILES_VIEW_NB_COL] = 11;

  ShowWindow(GetDlgItem(Tabl[TABL_FILES],TV_VIEW), SW_HIDE);
  ShowWindow(GetDlgItem(Tabl[TABL_FILES],LV_FILES_VIEW), SW_SHOW);

  lvc.cx = 60;       //taille colonne
  lvc.pszText = "File"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_VIEW,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_CONF,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 50;       //taille colonne
  lvc.pszText = "Key"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_VIEW,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_CONF,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 120;       //taille colonne
  lvc.pszText = "Value"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_VIEW,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_CONF,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 150;       //taille colonne
  lvc.pszText = "Data"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_VIEW,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_CONF,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 50;       //taille colonne
  lvc.pszText = "Type"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_VIEW,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);
  lvc.pszText = "Description"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_CONF,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);
  lvc.cx = 90;       //taille colonne
  lvc.pszText = "Parent key update"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_VIEW,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  lvc.cx = 100;
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_CONF,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_VIEW,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_CONF,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_REGISTRY_CONF_NB_COL] = 7;

  lvc.cx = 50;       //taille colonne
  lvc.pszText = "File"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LOGICIEL,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Key"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LOGICIEL,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Name"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LOGICIEL,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = "Publisher"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LOGICIEL,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Install date-Update"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LOGICIEL,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "UninstallString"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LOGICIEL,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Checked"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LOGICIEL,LVM_INSERTCOLUMN,(WPARAM)6, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LOGICIEL,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_REGISTRY_LOGICIEL_NB_COL] = 7;

  lvc.cx = 50;       //taille colonne
  lvc.pszText = "File"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 50;       //taille colonne
  lvc.pszText = "Key"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Name"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = "User : SID-RID"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 50;       //taille colonne
  lvc.pszText = "Description"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);
  lvc.cx = 40;       //taille colonne
  lvc.pszText = "Group"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  lvc.cx = 40;       //taille colonne
  lvc.pszText = "Last logon"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_INSERTCOLUMN,(WPARAM)6, (LPARAM)&lvc);
  lvc.cx = 40;       //taille colonne
  lvc.pszText = "Nb Connection"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_INSERTCOLUMN,(WPARAM)7, (LPARAM)&lvc);
  lvc.cx = 40;       //taille colonne
  lvc.pszText = "State"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_INSERTCOLUMN,(WPARAM)8, (LPARAM)&lvc);
  lvc.cx = 40;       //taille colonne
  lvc.pszText = "Password hash (Syskey protected)-Pwdump format (ready)"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_INSERTCOLUMN,(WPARAM)9, (LPARAM)&lvc);
  lvc.cx = 40;       //taille colonne
  lvc.pszText = "Shadow copy"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_INSERTCOLUMN,(WPARAM)10, (LPARAM)&lvc);
  lvc.cx = 0;       //taille colonne
  lvc.pszText = "F Binary data"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_INSERTCOLUMN,(WPARAM)11, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_REGISTRY_USERS_NB_COL] = 12;

  //hFONT
  //SendMessage(GetDlgItem(Tabl[TABL_REGISTRY],LV_REGISTRY_USERS), WM_SETFONT,(WPARAM)CreateFont(12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Lucida Console"), TRUE);

  lvc.cx = 50;       //taille colonne
  lvc.pszText = "File"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MAJ,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Key"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MAJ,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 70;       //taille colonne
  lvc.pszText = "Component"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MAJ,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Name"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MAJ,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 70;       //taille colonne
  lvc.pszText = "Description"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MAJ,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);
  lvc.cx = 70;       //taille colonne
  lvc.pszText = "Installed date-Update"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MAJ,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  lvc.cx = 70;       //taille colonne
  lvc.pszText = "Installed by"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MAJ,LVM_INSERTCOLUMN,(WPARAM)6, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MAJ,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_REGISTRY_MAJ_NB_COL] = 7;

  lvc.cx = 50;       //taille colonne
  lvc.pszText = "File"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_SERVICES,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Key"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_SERVICES,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = "Name"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_SERVICES,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 40;       //taille colonne
  lvc.pszText = "State"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_SERVICES,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Path"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_SERVICES,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);
  lvc.cx = 110;       //taille colonne
  lvc.pszText = "Description"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_SERVICES,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  lvc.cx = 50;       //taille colonne
  lvc.pszText = "Type"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_SERVICES,LVM_INSERTCOLUMN,(WPARAM)6, (LPARAM)&lvc);
  lvc.cx = 70;       //taille colonne
  lvc.pszText = "Update"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_SERVICES,LVM_INSERTCOLUMN,(WPARAM)7, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_SERVICES,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_REGISTRY_SERVICES_NB_COL] = 8;

  lvc.cx = 50;       //taille colonne
  lvc.pszText = "File"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_HISTORIQUE,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 50;       //taille colonne
  lvc.pszText = "Key"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_HISTORIQUE,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 180;       //taille colonne
  lvc.pszText = "Path"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_HISTORIQUE,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 50;       //taille colonne
  lvc.pszText = "Use count"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_HISTORIQUE,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Last GUID update"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_HISTORIQUE,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "User"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_HISTORIQUE,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_HISTORIQUE,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_REGISTRY_HISTORIQUE_NB_COL] = 6;

  lvc.cx = 60;       //taille colonne
  lvc.pszText = "File"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USB,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Key"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USB,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Name"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USB,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 170;       //taille colonne
  lvc.pszText = "Description"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USB,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Last use"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USB,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_USB,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_REGISTRY_USB_NB_COL] = 5;

  lvc.cx = 50;       //taille colonne
  lvc.pszText = "File"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_START,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Key"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_START,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 150;       //taille colonne
  lvc.pszText = "Value"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_START,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 230;       //taille colonne
  lvc.pszText = "Data"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_START,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_START,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_REGISTRY_START_NB_COL] = 4;

  lvc.cx = 50;       //taille colonne
  lvc.pszText = "File"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LAN,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Key"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LAN,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 50;       //taille colonne
  lvc.pszText = "Card"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LAN,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = "Ip"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LAN,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = "DNS"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LAN,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);
  lvc.cx = 70;       //taille colonne
  lvc.pszText = "Gateway"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LAN,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  lvc.cx = 50;       //taille colonne
  lvc.pszText = "DHCP"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LAN,LVM_INSERTCOLUMN,(WPARAM)6, (LPARAM)&lvc);
  lvc.cx = 50;      //taille colonne
  lvc.pszText = "Wi-Fi"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LAN,LVM_INSERTCOLUMN,(WPARAM)7, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_LAN,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_REGISTRY_LAN_NB_COL] = 8;

  lvc.cx = 50;       //taille colonne
  lvc.pszText = "File"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_PASSWORD,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MRU,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_PATH,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Key"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_PASSWORD,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MRU,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_PATH,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Value"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_PASSWORD,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MRU,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_PATH,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Data"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_PASSWORD,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_PATH,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 160;       //taille colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MRU,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 60;       //taille colonne
  lvc.pszText = "Password"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_PASSWORD,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);
  lvc.cx = 120;       //taille colonne
  lvc.pszText = "Description"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_PASSWORD,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MRU,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);

  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_PASSWORD,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_MRU,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  SendDlgItemMessage(Tabl[TABL_REGISTRY],LV_REGISTRY_PATH,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_REGISTRY_PASSWORD_NB_COL] = 6;
  NB_COLONNE_LV[LV_REGISTRY_MRU_NB_COL] = 5;
  NB_COLONNE_LV[LV_REGISTRY_PATH_NB_COL] = 4;

  TABL_ID_REG_VISIBLE = 0;

  lvc.cx = 40;       //taille colonne
  lvc.pszText = "Pid"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_PROCESS],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Process"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_PROCESS],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = "Path"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_PROCESS],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = "Commande"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_PROCESS],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Owner"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_PROCESS],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)4, (LPARAM)&lvc);
  lvc.cx = 120;       //taille colonne
  lvc.pszText = "Start date"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_PROCESS],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)5, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = "Network"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_PROCESS],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)6, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_PROCESS],LV_VIEW,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP);
  NB_COLONNE_LV[LV_PROCESS_VIEW_NB_COL] = 7;

  lvc.cx = 130;       //taille colonne
  lvc.pszText = "Start"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW_H,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 130;       //taille colonne
  lvc.pszText = "End"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW_H,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 140;       //taille colonne
  lvc.pszText = "Duration"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW_H,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 140;       //taille colonne
  lvc.pszText = "User/ACL"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW_H,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW_H,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_STATE_H_VIEW_NB_COL] = 4;

  //la deuxi�me lv
  lvc.cx = 120;       //taille colonne
  lvc.pszText = "Date"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 60;       //taille colonne
  lvc.pszText = "Source"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 220;       //taille colonne
  lvc.pszText = "Description"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 140;       //taille colonne
  lvc.pszText = "User/ACL"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);
  SendDlgItemMessage(Tabl[TABL_STATE],LV_VIEW,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_STATE_VIEW_NB_COL] = 4;

  lvc.cx = 100;       //taille colonne
  lvc.pszText = "Description"; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_INFO],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)0, (LPARAM)&lvc);
  lvc.cx = 150;       //taille colonne
  lvc.pszText = ""; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_INFO],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)1, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = ""; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_INFO],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)2, (LPARAM)&lvc);
  lvc.cx = 80;       //taille colonne
  lvc.pszText = ""; //texte de la colonne
  SendDlgItemMessage(Tabl[TABL_INFO],LV_VIEW,LVM_INSERTCOLUMN,(WPARAM)3, (LPARAM)&lvc);

  SendDlgItemMessage(Tabl[TABL_INFO],LV_VIEW,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_GRIDLINES);
  NB_COLONNE_LV[LV_INFO_VIEW_NB_COL] = 4;

  //treeview
  TRV_HTREEITEM[TRV_LOGS] = AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,"Audit logs",TVI_ROOT);
  TRV_HTREEITEM[TRV_FILES] = AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,"Files path",TVI_ROOT);
  TRV_HTREEITEM[TRV_REGISTRY] = AjouterItemTreeView(Tabl[TABL_CONF], TRV_CONF_TESTS,"Registry files",TVI_ROOT);

  ShowWindow(GetDlgItem(Tabl[TABL_REGISTRY],TV_VIEW), SW_HIDE);
  ShowWindow(GetDlgItem(Tabl[TABL_REGISTRY],LV_REGISTRY_VIEW), SW_SHOW);

  //init de la barre de status
  int TaillePart[4];
  TaillePart[0] = 150; // messages d'information
  TaillePart[1] = 300; // �tat du scanne logs
  TaillePart[2] = 450; // �tat du scanne fichiers
  TaillePart[3] = 600; // �tat du scanne registre
  SendDlgItemMessage(Tabl[TABL_MAIN],SB_MAIN,SB_SETPARTS,(WPARAM)4, (LPARAM)TaillePart);

  //chargement du fichier ini
  //gestion du chargement/cr�ation de fichiers ini
  char path[MAX_PATH+1];
  GetMyDirectory(path, MAX_PATH);
  strncat(path,CONF_FILE,MAX_PATH);
  strncat(path,"\0",MAX_PATH);

  char tmp_DOCUMENT[MAX_PATH];
  char tmp_VIDEO[MAX_PATH];
  char tmp_IMAGE[MAX_PATH];
  char tmp_EXE[MAX_PATH];
  char tmp_CRYPT[MAX_PATH];
  char tmp_MAIL[MAX_PATH];
  char tmp_OTHER[MAX_PATH];

  char tmp_SOURCE_CODE[MAX_PATH];
  char tmp_LINK[MAX_PATH];
  char tmp_COMPRESSE[MAX_PATH];
  char tmp_WEB[MAX_PATH];
  char tmp_CONFIGURATION[MAX_PATH];
  char tmp_BDD[MAX_PATH];
  char tmp_AUDIT[MAX_PATH];

  memset(tmp_DOCUMENT, 0, MAX_PATH);
  memset(tmp_VIDEO, 0, MAX_PATH);
  memset(tmp_IMAGE, 0, MAX_PATH);
  memset(tmp_EXE, 0, MAX_PATH);
  memset(tmp_CRYPT, 0, MAX_PATH);
  memset(tmp_MAIL, 0, MAX_PATH);
  memset(tmp_OTHER, 0, MAX_PATH);

  memset(tmp_SOURCE_CODE, 0, MAX_PATH);
  memset(tmp_LINK, 0, MAX_PATH);
  memset(tmp_COMPRESSE, 0, MAX_PATH);
  memset(tmp_WEB, 0, MAX_PATH);
  memset(tmp_CONFIGURATION, 0, MAX_PATH);
  memset(tmp_BDD, 0, MAX_PATH);
  memset(tmp_AUDIT, 0, MAX_PATH);

  //test si le fichier ini existe
  if (FichierExiste(path))
  {
    GetPrivateProfileString("FILES","OFFICE","doc,docx,docm,pdf,xls,xlsx,xlsm,odp,odt,ods,ppt,pptx,pptm,pps,rtf,sdw,csv,txt,chm,mht,hlp",tmp_DOCUMENT,MAX_PATH,path);
    GetPrivateProfileString("FILES","VIDEO","avi,mpg,mpeg,mp3,wma,wmv,flv,ogg,ogm,mp4,mkv,wav",tmp_VIDEO,MAX_PATH,path);
    GetPrivateProfileString("FILES","IMAGE","jpg,jpeg,bmp,png,gif,ico",tmp_IMAGE,MAX_PATH,path);
    GetPrivateProfileString("FILES","APPLICATION","exe,com,cmd,ps1,ps2,vbs,bat,pl,py,rb,sh",tmp_EXE,MAX_PATH,path);
    GetPrivateProfileString("FILES","CRYPT","tc,zed,gpg,pgp,cry,box,axx",tmp_CRYPT,MAX_PATH,path);
    GetPrivateProfileString("FILES","MAIL","msf,eml,dbx",tmp_MAIL,MAX_PATH,path);

    GetPrivateProfileString("FILES","SOURCE_CODE","c,cpp,h,hpp,rc,dev,o,res,layout,a,depend,cs,vb",tmp_SOURCE_CODE,MAX_PATH,path);
    GetPrivateProfileString("FILES","LINK","lnk",tmp_LINK,MAX_PATH,path);
    GetPrivateProfileString("FILES","COMPRESSE","zip,rar,arj,tar,gz,bzip,7z,wim,tgz,ace",tmp_COMPRESSE,MAX_PATH,path);
    GetPrivateProfileString("FILES","WEB","htm,html,xml,xsl,css,js,asm,dtd,php,asp,asp",tmp_WEB,MAX_PATH,path);
    GetPrivateProfileString("FILES","CONFIGURATION","cfg,ini,conf,reg",tmp_CONFIGURATION,MAX_PATH,path);
    GetPrivateProfileString("FILES","BDD","db,sqlite",tmp_BDD,MAX_PATH,path);
    GetPrivateProfileString("FILES","AUDIT","log,evt,evtx",tmp_AUDIT,MAX_PATH,path);

    GetPrivateProfileString("FILES","OTHER","",tmp_OTHER,MAX_PATH,path);
  }else
  {
    //chargement manuel en m�moire et sauvegarde dans fichier de configuration
    WritePrivateProfileString("FILES","OFFICE","doc,docx,docm,pdf,xls,xlsx,xlsm,odp,odt,ods,ppt,pptx,pptm,pps,rtf,sdw,csv,txt,chm,mht,hlp",path);
    WritePrivateProfileString("FILES","VIDEO","avi,mpg,mpeg,mp3,wma,wmv,flv,ogg,ogm,mp4,mkv,wav",path);
    WritePrivateProfileString("FILES","IMAGE","jpg,jpeg,bmp,png,gif,ico",path);
    WritePrivateProfileString("FILES","APPLICATION","exe,com,cmd,ps1,ps2,vbs,bat,pl,py,rb,sh",path);
    WritePrivateProfileString("FILES","CRYPT","tc,zed,gpg,pgp,cry,box,axx",path);
    WritePrivateProfileString("FILES","MAIL","msf,eml,dbx",path);

    WritePrivateProfileString("FILES","SOURCE_CODE","c,cpp,h,hpp,rc,dev,o,res,layout,a,depend,cs,vb",path);
    WritePrivateProfileString("FILES","LINK","lnk",path);
    WritePrivateProfileString("FILES","COMPRESSE","zip,rar,arj,tar,gz,bzip,7z,wim,tgz,ace",path);
    WritePrivateProfileString("FILES","WEB","htm,html,xml,xsl,css,js,asm,dtd,php,asp,asp",path);
    WritePrivateProfileString("FILES","CONFIGURATION","cfg,ini,conf,reg",path);
    WritePrivateProfileString("FILES","BDD","db,sqlite",path);
    WritePrivateProfileString("FILES","AUDIT","log,evt,evtx",path);

    WritePrivateProfileString("FILES","OTHER","",path);

    //chargement en m�moire
    strcpy(tmp_DOCUMENT,"doc,docx,docm,pdf,xls,xlsx,xlsm,odp,odt,ods,ppt,pptx,pptm,pps,rtf,sdw,csv,txt,chm,mht,hlp");
    strcpy(tmp_VIDEO,"avi,mpg,mpeg,mp3,wma,wmv,flv,ogg,ogm,mp4,mkv,wav");
    strcpy(tmp_IMAGE,"jpg,jpeg,bmp,png,gif,ico");
    strcpy(tmp_EXE,"exe,com,cmd,ps1,ps2,vbs,bat,pl,py,rb,sh");
    strcpy(tmp_CRYPT,"tc,zed,gpg,pgp,cry,box,axx");
    strcpy(tmp_MAIL,"msf,eml,dbx");

    strcpy(tmp_SOURCE_CODE,"c,cpp,h,hpp,rc,dev,o,res,layout,a,depend,cs,vb");
    strcpy(tmp_LINK,"lnk");
    strcpy(tmp_COMPRESSE,"zip,rar,arj,tar,gz,bzip,7z,wim,tgz,ace");
    strcpy(tmp_WEB,"htm,html,xml,xsl,css,js,asm,dtd,php,asp,aspx");
    strcpy(tmp_CONFIGURATION,"cfg,ini,conf,reg");
    strcpy(tmp_BDD,"db,sqlite");
    strcpy(tmp_AUDIT,"log,evt,evtx");

    strcpy(tmp_OTHER,"");
  }

  //traitement des donn�es pour ajout en m�moire ^^
  nb_ext_doc = TraitementChaineExt(ext_doc,tmp_DOCUMENT,NB_MAX_EXT_TYPE);
  nb_ext_med = TraitementChaineExt(ext_med,tmp_VIDEO,NB_MAX_EXT_TYPE);
  nb_ext_img = TraitementChaineExt(ext_img,tmp_IMAGE,NB_MAX_EXT_TYPE);
  nb_ext_exe = TraitementChaineExt(ext_exe,tmp_EXE,NB_MAX_EXT_TYPE);
  nb_ext_crypt = TraitementChaineExt(ext_crypt,tmp_CRYPT,NB_MAX_EXT_TYPE);
  nb_ext_mail = TraitementChaineExt(ext_mail,tmp_MAIL,NB_MAX_EXT_TYPE);
  nb_ext_other = TraitementChaineExt(ext_other,tmp_OTHER,NB_MAX_EXT_TYPE);

  nb_ext_source_code    = TraitementChaineExt(ext_source_code   ,tmp_SOURCE_CODE,NB_MAX_EXT_TYPE);
  nb_ext_lnk            = TraitementChaineExt(ext_link          ,tmp_LINK,NB_MAX_EXT_TYPE);
  nb_ext_compresse      = TraitementChaineExt(ext_compresse     ,tmp_COMPRESSE,NB_MAX_EXT_TYPE);
  nb_ext_web            = TraitementChaineExt(ext_web           ,tmp_WEB,NB_MAX_EXT_TYPE);
  nb_ext_configuration  = TraitementChaineExt(ext_configurationr,tmp_CONFIGURATION,NB_MAX_EXT_TYPE);
  nb_ext_bdd            = TraitementChaineExt(ext_bdd           ,tmp_BDD,NB_MAX_EXT_TYPE);
  nb_ext_audit          = TraitementChaineExt(ext_audit         ,tmp_AUDIT,NB_MAX_EXT_TYPE);

  //pr�paration � la recherche de filtre pour registre
  unsigned int i=0;
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows\\CurrentVersion\\Run");
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows\\CurrentVersion\\RunOnce");
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows\\CurrentVersion\\RunOnceEx");
  strcpy(ref_autorun_search[i++].v,"Policies\\Microsoft\\Windows\\System\\Scripts\\Startup");
  strcpy(ref_autorun_search[i++].v,"Policies\\Microsoft\\Windows\\System\\Scripts\\Logon");
  strcpy(ref_autorun_search[i++].v,"Policies\\Microsoft\\Windows\\System\\Scripts\\Shutdown");
  strcpy(ref_autorun_search[i++].v,"Policies\\Microsoft\\Windows\\System\\Scripts\\Logoff");
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows NT\\CurrentVersion\\Windows\\Load");
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows NT\\CurrentVersion\\Windows\\Run");
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows NT\\CurrentVersion\\Windows\\AppSetup");
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows NT\\CurrentVersion\\Windows\\Shell");
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows\\CurrentVersion\\Group Policy\\Scripts\\Startup");
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows\\CurrentVersion\\Group Policy\\Scripts\\Shutdow");
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows\\CurrentVersion\\Policies\\System\\Shell");
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\Run");
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows CE Services\\AutoStartOnConnect");
  strcpy(ref_autorun_search[i++].v,"Microsoft\\Windows CE Services\\AutoStartOnDisconnect");

  i=0;
  strcpy(ref_hiddenlog_search[i++].v,"Explorer\\UserAssist\\{5E6AB780-7743-11CF-A12B-00AA004AE837}\\Count");
  strcpy(ref_hiddenlog_search[i++].v,"Explorer\\UserAssist\\{75048700-EF1F-11D0-9888-006097DEACF9}\\Count");

  i=0;
  strcpy(ref_software_search[i++].v,"Microsoft\\Windows\\CurrentVersion\\Uninstall\\");
  i=0;
  strcpy(ref_software_var_search[i++].v,"DisplayName");
  strcpy(ref_software_var_search[i++].v,"Publisher");
  strcpy(ref_software_var_search[i++].v,"InstallDate");
  strcpy(ref_software_var_search[i++].v,"UninstallString");

  i=0;
  strcpy(ref_update_search[i++].v,"Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\Packages\\");
  strcpy(ref_update_search[i++].v,"Microsoft\\Updates\\");

  i=0;
  strcpy(ref_usb_search[i++].v,"Enum\\USBSTOR\\");

  i=0;
  strcpy(ref_network_search[i++].v,"Services\\Tcpip\\Parameters\\Interfaces\\");

  i=0;
  strcpy(ref_service_search[i++].v,"Services\\");
  i=0;
  strcpy(ref_service_var_search[i++].v,"DisplayName");
  strcpy(ref_service_var_search[i++].v,"Start");
  strcpy(ref_service_var_search[i++].v,"ImagePath");
  strcpy(ref_service_var_search[i++].v,"Description");
  strcpy(ref_service_var_search[i++].v,"Type");

  i=0;
  strcpy(ref_users_search[i++].v,"Account\\Users\\");

  i=0;
  strcpy(ref_conf_search[i++].v,"\\Windows NT\\CurrentVersion");
  strcpy(ref_conf_search[i++].v,"CurrentVersion\\Policies\\Explorer");
  strcpy(ref_conf_search[i++].v,"Services\\LanManServer\\Parameters");
  strcpy(ref_conf_search[i++].v,"Services\\LanManWorkstation\\Parameters");
  strcpy(ref_conf_search[i++].v,"\\Control\\Lsa");
  strcpy(ref_conf_search[i++].v,"CurrentVersion\\Policies\\System");

  i=0;
  strcpy(ref_conf_var_search[i++].v,"ProductName");
  strcpy(ref_conf_var_search[i++].v,"(configuration) Operating System");
  strcpy(ref_conf_var_search[i++].v,"CSDVersion");
  strcpy(ref_conf_var_search[i++].v,"(configuration) Service Pack");
  strcpy(ref_conf_var_search[i++].v,"SystemRoot");
  strcpy(ref_conf_var_search[i++].v,"(configuration) System path");

  strcpy(ref_conf_var_search[i++].v,"AppInit_DLLs");
  strcpy(ref_conf_var_search[i++].v,"(malware) DLL load in GUI Windows");
  strcpy(ref_conf_var_search[i++].v,"NoDriveTypeAutoRun");
  strcpy(ref_conf_var_search[i++].v,"(malware) Autorun usb/CDROM/..., 0xff = disable");
  strcpy(ref_conf_var_search[i++].v,"Autorun");
  strcpy(ref_conf_var_search[i++].v,"(malware) Autorun CDROM, 0x00 = disable");
  strcpy(ref_conf_var_search[i++].v,"EnableLUA");
  strcpy(ref_conf_var_search[i++].v,"(malware) 0x01:Disable UAC (User Account Control)");
  strcpy(ref_conf_var_search[i++].v,"disablecad");
  strcpy(ref_conf_var_search[i++].v,"(malware) 0x01:Enable CTRL+ALT+DEL requirement for logon");

  strcpy(ref_conf_var_search[i++].v,"Debugger");
  strcpy(ref_conf_var_search[i++].v,"(attack vector) Use for redirect application");
  strcpy(ref_conf_var_search[i++].v,"RestrictNullSessAccess");
  strcpy(ref_conf_var_search[i++].v,"(attack vector) Null session, 0x01:Disable");
  strcpy(ref_conf_var_search[i++].v,"RestrictAnonymousSAM");
  strcpy(ref_conf_var_search[i++].v,"(attack vector) Anonymous connexion for SAM enumeration (revers SID), 0x01:Disable");
  strcpy(ref_conf_var_search[i++].v,"RestrictAnonymous");
  strcpy(ref_conf_var_search[i++].v,"(attack vector) Anonymous connexion, 0x01:Disable");
  strcpy(ref_conf_var_search[i++].v,"DontDisplayLastUserName");
  strcpy(ref_conf_var_search[i++].v,"(attack vector) Display last user login, 0x01:Disable");
  strcpy(ref_conf_var_search[i++].v,"DisableRegistryTools");
  strcpy(ref_conf_var_search[i++].v,"(attack vector) Disable use of windows registry editor, 0x01:Disable");
  strcpy(ref_conf_var_search[i++].v,"restrictanonymous");
  strcpy(ref_conf_var_search[i++].v,"(attack vector) Additional restrictions for anonymous connections, 0x01:default, 0x01:Do not allow enumeration of SAM accounts and shares, 0x02:No access without explicit permissions.");

  strcpy(ref_conf_var_search[i++].v,"LmCompatibilityLevel");
  strcpy(ref_conf_var_search[i++].v,"(authentication) Authentication method 0x00:only LM or NTLM, 0x01:NTLMv2 if enable, 0x02:NTLM only, 0x03:NTLMv2 only, 0x04:LM deny, 0x05:LM and NTLM deny, NTLMv2 only");
  strcpy(ref_conf_var_search[i++].v,"NoLMHash");
  strcpy(ref_conf_var_search[i++].v,"(authentication) 0x01:Disable LM HASH");
  strcpy(ref_conf_var_search[i++].v,"enableplaintextpassword");
  strcpy(ref_conf_var_search[i++].v,"(authentication) 0x00:Disable send unencrypted password to connect to third-party SMB servers");
  strcpy(ref_conf_var_search[i++].v,"dontdisplaylastusername");
  strcpy(ref_conf_var_search[i++].v,"(authentication) 0x01:Do not display last username in logon screen");
  strcpy(ref_conf_var_search[i++].v,"legalnoticetext");
  strcpy(ref_conf_var_search[i++].v,"(authentication) Message title for user attempting to log on");
  strcpy(ref_conf_var_search[i++].v,"legalnoticecaption");
  strcpy(ref_conf_var_search[i++].v,"(authentication) Message for user attempting to log on");

  strcpy(ref_conf_var_search[i++].v,"auditbaseobjects");
  strcpy(ref_conf_var_search[i++].v,"(audit) Audit the access of global system objects, 0x01:Enable");
  strcpy(ref_conf_var_search[i++].v,"fullprivilegeauditing");
  strcpy(ref_conf_var_search[i++].v,"(audit) Audit use of Backup and Restore privilege, 0x01:Enable");

  i=0;
  strcpy(ref_mru_search[i++].v,"CurrentVersion\\Explorer\\RunMRU");
  strcpy(ref_mru_search[i++].v,"CurrentVersion\\Explorer\\ComDlg32\\OpenSaveMRU");
  strcpy(ref_mru_search[i++].v,"Microsoft\\MediaPlayer\\Player\\RecentFileList");
  strcpy(ref_mru_search[i++].v,"Microsoft\\MediaPlayer\\Player\\RecentURLList");
  strcpy(ref_mru_search[i++].v,"Publisher\\Recent File List");
  strcpy(ref_mru_search[i++].v,"PowerPoint\\Recent File List");
  strcpy(ref_mru_search[i++].v,"Excel\\Recent File");
  strcpy(ref_mru_search[i++].v,"Word\\Data");
  strcpy(ref_mru_search[i++].v,"FrontPage Explorer\\Recent File List");
  strcpy(ref_mru_search[i++].v,"Microsoft\\Search Assistant\\ACMru\\5001");
  strcpy(ref_mru_search[i++].v,"Microsoft\\Search Assistant\\ACMru\\5603");
  strcpy(ref_mru_search[i++].v,"Microsoft\\Search Assistant\\ACMru\\5604");
  strcpy(ref_mru_search[i++].v,"Microsoft\\Search Assistant\\ACMru\\5647");
  strcpy(ref_mru_search[i++].v,"Windows\\CurrentVersion\\Explorer\\Map Network Drive MRU");
  strcpy(ref_mru_search[i++].v,"Microsoft\\Windows\\CurrentVersion\\Applets\\Paint\\Recent File List");
  strcpy(ref_mru_search[i++].v,"Microsoft\\Windows\\CurrentVersion\\Applets\\Wordpad\\Recent File List");
  strcpy(ref_mru_search[i++].v,"Microsoft\\Windows\\CurrentVersion\\Explorer\\ComputerDescriptions");
  strcpy(ref_mru_search[i++].v,"Microsoft\\Internet Explorer\\TypedURLs");
  strcpy(ref_mru_search[i++].v,"wnd_hex_Main\\Twnd_hex_Main.MRUMan");
  strcpy(ref_mru_search[i++].v,"Main\\MRUHistory");
  strcpy(ref_mru_search[i++].v,"Foxit Software\\Foxit Reader 1.0\\Recent File List");
  strcpy(ref_mru_search[i++].v,"Foxit Software\\Foxit Reader 2.0\\Recent File List");
  strcpy(ref_mru_search[i++].v,"Foxit Software\\Foxit Reader 3.0\\Recent File List");
  strcpy(ref_mru_search[i++].v,"Foxit Software\\Foxit Reader 4.0\\Recent File List");
  strcpy(ref_mru_search[i++].v,"Foxit Software\\Foxit Reader 5.0\\Recent File List");
  strcpy(ref_mru_search[i++].v,"Foxit Software\\Foxit Reader 6.0\\Recent File List");
  strcpy(ref_mru_search[i++].v,"Morpheus\\GUI\\SearchRecent");
  strcpy(ref_mru_search[i++].v,"Foxit Software\\Foxit Reader 5.0\\Recent File List");
  strcpy(ref_mru_search[i++].v,"ShellNoRoam\\MUICache");
  strcpy(ref_mru_search[i++].v,"Shell\\MuiCache");
  strcpy(ref_mru_search[i++].v,"Shell\\LocalizedResourceName");

  i=0;
  strcpy(ref_mru_var_search[i++].v,"MRUList");
  strcpy(ref_mru_var_search[i++].v,"Open history");
  strcpy(ref_mru_var_search[i++].v,"MRUList");
  strcpy(ref_mru_var_search[i++].v,"Save history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Media player history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Media player history");
  strcpy(ref_mru_var_search[i++].v,"MRUList");
  strcpy(ref_mru_var_search[i++].v,"Publisher history");
  strcpy(ref_mru_var_search[i++].v,"MRUList");
  strcpy(ref_mru_var_search[i++].v,"PowerPoint history");
  strcpy(ref_mru_var_search[i++].v,"MRUList");
  strcpy(ref_mru_var_search[i++].v,"Excel history");
  strcpy(ref_mru_var_search[i++].v,"MRUList");
  strcpy(ref_mru_var_search[i++].v,"Word history");
  strcpy(ref_mru_var_search[i++].v,"MRUList");
  strcpy(ref_mru_var_search[i++].v,"Frontpage history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Internet search assistant history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Search assistant history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Search picture, music and video history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Search printers, computers and people history");
  strcpy(ref_mru_var_search[i++].v,"MRUList");
  strcpy(ref_mru_var_search[i++].v,"Map network drive history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Paint history");
  strcpy(ref_mru_var_search[i++].v,"Toolsbars");
  strcpy(ref_mru_var_search[i++].v,"Wordpad history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Connected network device history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Internet Explorer URL history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"MiTeC HexEditor history");
  strcpy(ref_mru_var_search[i++].v,"MaxFiles");
  strcpy(ref_mru_var_search[i++].v,"MiTeC WRR history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Foxit reader history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Foxit reader history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Foxit reader history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Foxit reader history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Foxit reader history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"Foxit reader history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"P2P - Morpheus search history");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"MUICache");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"MUICache");
  strcpy(ref_mru_var_search[i++].v,"");
  strcpy(ref_mru_var_search[i++].v,"MUICache");

//FireWall configuration
//HKLM\SYSTEM\ControlSet001\Services\SharedAccess\Parameters\FirewallPolicy\StandardProfile\AuthorizedApplications\List

//R. Desktop � Connect 	Software\Microsoft\Terminal Server Client\Default [MRUnumber]

  //Unicode/Wildstring
  i=0;
  strcpy(ref_mru_wild_search[i++].v,"CurrentVersion\\Explorer\\ComDlg32\\LastVisitedMRU");
  strcpy(ref_mru_wild_search[i++].v,"CurrentVersion\\Explorer\\RecentDocs");
  strcpy(ref_mru_wild_search[i++].v,"CurrentVersion\\Explorer\\StreamMRU");


  i=0;
  strcpy(ref_mru_var_wils_search[i++].v,"MRUList");
  strcpy(ref_mru_var_wils_search[i++].v,"Command history");
  strcpy(ref_mru_var_wils_search[i++].v,"MRUListEx");
  strcpy(ref_mru_var_wils_search[i++].v,"Recent history");
  strcpy(ref_mru_var_wils_search[i++].v,"MRUListEx");
  strcpy(ref_mru_var_wils_search[i++].v,"Stream history");
  current_key_test[0]=0;
  item_ref_current_key_test=-1;

  //gestion du mode console
  if (consol_mode && strlen(console_cmd)>4)
  {
    //on masque la fen�tre pincipale
    ShowWindow(hwnd, SW_HIDE);
    ScanStart = TRUE;

    //lecture du 1er path (suppression des ")
    char path_f[MAX_LINE_SIZE];
    char *c;
    snprintf(path_f,MAX_LINE_SIZE, console_cmd+3);
    c =path_f;
    while (*c && *c !='"')c++;

    //si un deuxi�me lecture (suppression des ")
    char path_s[MAX_LINE_SIZE];
    snprintf(path_s,MAX_LINE_SIZE, c+3);
    *c =0;
    c =path_s;
    while (*c && *c !='"')c++;
    *c =0;

 /*
      case 'h': MessageBox(0,"Read to Catch All :\n"
                        "Licensed under the terms of the GNU\n"
                        "General Public License version 3.\n\n"
                        "Author: Nicolas Hanteville\n"
                        "Home: http://code.google.com/p/omnia-projetcs/\n"
                        "----------------------------------\n"
                        "Uses:\n\n"
                        "h : Display this help.\n\n"

                        "r : Read recursively all files and ACL from all system directory and export to CSV/XML/HTML.\n"
                        "    example: r \"c:\\file_to_save.csv\"\n\n"
                        "e : Read recursively all files and ACL from directory and export to CSV/XML/HTML.\n"
                        "    example: e \"c:\\directory_to_read\" \"c:\\file_to_save.csv\"\n\n"

                        "p : Read all process and informations and export to CSV/XML/HTML.\n"
                        "    example: p \"c:\\file_to_save.csv\"\n\n"

                        "a : Read all audit logs and export to CSV/XML/HTML.\n"
                        "    example: a \"c:\\file_to_save.csv\"\n\n"
                        "l : Read audit file (evt, evtx) and export to CSV/XML/HTML.\n"
                        "    example: l \"c:\\file_to_read.evt\" \"c:\\file_to_save.csv\"\n\n"

                        "g : Read local registry and export to CSV/XML/HTML.\n"
                        "    example: g \"c:\\directory_to_save\\\"\n\n"
                        "f : Read registry file (reg, binary registry) and export to CSV.\n"
                        "    example: f \"c:\\file_to_read.reg\" \"c:\\directory_to_save\\\"\n\n"
                        "s : Read SAM+SYTEM binary registry from directory and export to CSV.\n"
                        "    example: f \"c:\\SAM_end_SYSTEM_directory\\\" \"c:\\directory_to_save\\\""

                        ,"HELP",MB_OK|MB_TOPMOST);
 */

    switch(console_cmd[0])
    {
      //file ACL
      case 'r':
        //lire toutes les ACLs de tous les fichiers
        Scan_files((LPVOID)TRUE);
        //export
        switch(path_f[strlen(path_f)-3])
        {
          case 'x':
          case 'X':ExportLVtoXML(path_f, TABL_FILES, LV_FILES_VIEW, NB_COLONNE_LV[LV_FILES_VIEW_NB_COL]);break;
          case 't':
          case 'T':ExportLVtoHTML(path_f, TABL_FILES, LV_FILES_VIEW, NB_COLONNE_LV[LV_FILES_VIEW_NB_COL]);break;
          default:ExportLVtoCSV(path_f, TABL_FILES, LV_FILES_VIEW, NB_COLONNE_LV[LV_FILES_VIEW_NB_COL]);break;
        }
      break;
      case 'e':
        //ajout de r�pertoire ^^
        FileToTreeView(path_f);
        //lire les ACLS
        Scan_files(FALSE);
        //export
        switch(path_s[strlen(path_s)-3])
        {
          case 'x':
          case 'X':ExportLVtoXML(path_s, TABL_FILES, LV_FILES_VIEW, NB_COLONNE_LV[LV_FILES_VIEW_NB_COL]);break;
          case 't':
          case 'T':ExportLVtoHTML(path_s, TABL_FILES, LV_FILES_VIEW, NB_COLONNE_LV[LV_FILES_VIEW_NB_COL]);break;
          default:ExportLVtoCSV(path_s, TABL_FILES, LV_FILES_VIEW, NB_COLONNE_LV[LV_FILES_VIEW_NB_COL]);break;
        }
      break;

      //Process
      case 'p':
        //�num�ration des process
        EnumProcess(GetDlgItem(Tabl[TABL_PROCESS],LV_VIEW), NB_COLONNE_LV[LV_PROCESS_VIEW_NB_COL]);
        //export
        switch(path_f[strlen(path_f)-3])
        {
          case 'x':
          case 'X':ExportLVtoXML(path_f, TABL_PROCESS, LV_VIEW, NB_COLONNE_LV[LV_PROCESS_VIEW_NB_COL]);break;
          case 't':
          case 'T':ExportLVtoHTML(path_f, TABL_PROCESS, LV_VIEW, NB_COLONNE_LV[LV_PROCESS_VIEW_NB_COL]);break;
          default:ExportLVtoCSV(path_f, TABL_PROCESS, LV_VIEW, NB_COLONNE_LV[LV_PROCESS_VIEW_NB_COL]);break;
        }
      break;

      //audit log
      case 'a':
        //audit
        Scan_logs((LPVOID)TRUE);
        //export
        switch(path_f[strlen(path_f)-3])
        {
          case 'x':
          case 'X':ExportLVtoXML(path_f, TABL_LOGS, LV_VIEW, NB_COLONNE_LV[LV_LOGS_VIEW_NB_COL]);break;
          case 't':
          case 'T':ExportLVtoHTML(path_f, TABL_LOGS, LV_VIEW, NB_COLONNE_LV[LV_LOGS_VIEW_NB_COL]);break;
          default:ExportLVtoCSV(path_f, TABL_LOGS, LV_VIEW, NB_COLONNE_LV[LV_LOGS_VIEW_NB_COL]);break;
        }
      break;
      case 'l':
        //ajout de fichiers ^^
        FileToTreeView(path_f);
        //audit
        Scan_logs(FALSE);
        //export
        switch(path_s[strlen(path_s)-3])
        {
          case 'x':
          case 'X':ExportLVtoXML(path_s, TABL_LOGS, LV_LOGS_VIEW, NB_COLONNE_LV[LV_LOGS_VIEW_NB_COL]);break;
          case 't':
          case 'T':ExportLVtoHTML(path_s, TABL_LOGS, LV_LOGS_VIEW, NB_COLONNE_LV[LV_LOGS_VIEW_NB_COL]);break;
          default:ExportLVtoCSV(path_s, TABL_LOGS, LV_LOGS_VIEW, NB_COLONNE_LV[LV_LOGS_VIEW_NB_COL]);break;
        }
      break;

      //registry
      case 'g':
      {
        //registry
        Scan_registry((LPVOID)TRUE);
        //export
        char *c,tmp[MAX_PATH]="",path[MAX_PATH]="";
        strcpy(path,path_f);
        //cr�ation du path initial + date
        strncat(path,"RTCA[\0",MAX_PATH);
        time_t dateEtHMs;
        time(&dateEtHMs);
        //on supprime les caract�res incompatibles avec le nom d'un fichier
        strncpy(tmp,(char *)ctime(&dateEtHMs),MAX_PATH);
        tmp[strlen(tmp)-1]=0;
        c = tmp;
        *(c+=3) = '_';
        *(c+=4) = '_';
        *(c+=3) = '_';
        *(c+=3) = 'h';
        *(c+=3) = 'm';
        *(c+=3) = '_';
        strncat(path,tmp,MAX_PATH);
        strncat(path,"]\0",MAX_PATH);

        //cr�ation des rapports
        strcpy(tmp,path);
        strncat(tmp,"_REG_REGISTRY.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_VIEW, NB_COLONNE_LV[LV_REGISTRY_CONF_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_CONGIGURATION.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_CONF, NB_COLONNE_LV[LV_REGISTRY_CONF_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_SOFTWARE.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_LOGICIEL, NB_COLONNE_LV[LV_REGISTRY_LOGICIEL_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_UPDATE.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_MAJ, NB_COLONNE_LV[LV_REGISTRY_MAJ_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_SERVICES_DRIVERS.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_SERVICES, NB_COLONNE_LV[LV_REGISTRY_SERVICES_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_USERASSIST.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_HISTORIQUE, NB_COLONNE_LV[LV_REGISTRY_HISTORIQUE_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_USB.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_USB, NB_COLONNE_LV[LV_REGISTRY_USB_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_START.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_START, NB_COLONNE_LV[LV_REGISTRY_START_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_NETWORK.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_LAN, NB_COLONNE_LV[LV_REGISTRY_LAN_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_USERS.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_USERS, NB_COLONNE_LV[LV_REGISTRY_USERS_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_PASSWORD.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_PASSWORD, NB_COLONNE_LV[LV_REGISTRY_PASSWORD_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_MRU.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_MRU, NB_COLONNE_LV[LV_REGISTRY_MRU_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_Path.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_PATH, NB_COLONNE_LV[LV_REGISTRY_PATH_NB_COL]);
      }
      break;
      case 'f':
      {
        //ajout de fichiers ^^
        FileToTreeView(path_f);
        //registry
        Scan_registry(FALSE);
        //export
        char *c,tmp[MAX_PATH]="",path[MAX_PATH]="";
        strcpy(path,path_s);
        //cr�ation du path initial + date
        strncat(path,"RTCA[\0",MAX_PATH);
        time_t dateEtHMs;
        time(&dateEtHMs);
        //on supprime les caract�res incompatibles avec le nom d'un fichier
        strncpy(tmp,(char *)ctime(&dateEtHMs),MAX_PATH);
        tmp[strlen(tmp)-1]=0;
        c = tmp;
        *(c+=3) = '_';
        *(c+=4) = '_';
        *(c+=3) = '_';
        *(c+=3) = 'h';
        *(c+=3) = 'm';
        *(c+=3) = '_';
        strncat(path,tmp,MAX_PATH);
        strncat(path,"]\0",MAX_PATH);

        //cr�ation des rapports
        strcpy(tmp,path);
        strncat(tmp,"_REG_REGISTRY.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_VIEW, NB_COLONNE_LV[LV_REGISTRY_CONF_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_CONGIGURATION.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_CONF, NB_COLONNE_LV[LV_REGISTRY_CONF_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_SOFTWARE.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_LOGICIEL, NB_COLONNE_LV[LV_REGISTRY_LOGICIEL_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_UPDATE.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_MAJ, NB_COLONNE_LV[LV_REGISTRY_MAJ_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_SERVICES_DRIVERS.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_SERVICES, NB_COLONNE_LV[LV_REGISTRY_SERVICES_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_USERASSIST.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_HISTORIQUE, NB_COLONNE_LV[LV_REGISTRY_HISTORIQUE_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_USB.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_USB, NB_COLONNE_LV[LV_REGISTRY_USB_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_START.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_START, NB_COLONNE_LV[LV_REGISTRY_START_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_NETWORK.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_LAN, NB_COLONNE_LV[LV_REGISTRY_LAN_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_USERS.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_USERS, NB_COLONNE_LV[LV_REGISTRY_USERS_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_PASSWORD.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_PASSWORD, NB_COLONNE_LV[LV_REGISTRY_PASSWORD_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_MRU.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_MRU, NB_COLONNE_LV[LV_REGISTRY_MRU_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_Path.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_PATH, NB_COLONNE_LV[LV_REGISTRY_PATH_NB_COL]);
      }
      break;
      case 's':
      {
        char *c,tmp[MAX_PATH]="",path[MAX_PATH]="";
        strcpy(path,path_s);

        //ajout de fichiers ^^
        snprintf(tmp,MAX_PATH,"%sSYSTEM",path_f);
        FileToTreeView(tmp);
        snprintf(tmp,MAX_PATH,"%sSAM",path_f);
        FileToTreeView(tmp);

        //registry
        Scan_registry(FALSE);

        //export
        //cr�ation du path initial + date
        strncat(path,"RTCA[\0",MAX_PATH);
        time_t dateEtHMs;
        time(&dateEtHMs);
        //on supprime les caract�res incompatibles avec le nom d'un fichier
        strncpy(tmp,(char *)ctime(&dateEtHMs),MAX_PATH);
        tmp[strlen(tmp)-1]=0;
        c = tmp;
        *(c+=3) = '_';
        *(c+=4) = '_';
        *(c+=3) = '_';
        *(c+=3) = 'h';
        *(c+=3) = 'm';
        *(c+=3) = '_';
        strncat(path,tmp,MAX_PATH);
        strncat(path,"]\0",MAX_PATH);

        //cr�ation des rapports
        strcpy(tmp,path);
        strncat(tmp,"_REG_REGISTRY.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_VIEW, NB_COLONNE_LV[LV_REGISTRY_CONF_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_CONGIGURATION.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_CONF, NB_COLONNE_LV[LV_REGISTRY_CONF_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_SOFTWARE.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_LOGICIEL, NB_COLONNE_LV[LV_REGISTRY_LOGICIEL_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_UPDATE.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_MAJ, NB_COLONNE_LV[LV_REGISTRY_MAJ_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_SERVICES_DRIVERS.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_SERVICES, NB_COLONNE_LV[LV_REGISTRY_SERVICES_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_USERASSIST.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_HISTORIQUE, NB_COLONNE_LV[LV_REGISTRY_HISTORIQUE_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_USB.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_USB, NB_COLONNE_LV[LV_REGISTRY_USB_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_START.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_START, NB_COLONNE_LV[LV_REGISTRY_START_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_NETWORK.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_LAN, NB_COLONNE_LV[LV_REGISTRY_LAN_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_USERS.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_USERS, NB_COLONNE_LV[LV_REGISTRY_USERS_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_PASSWORD.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_PASSWORD, NB_COLONNE_LV[LV_REGISTRY_PASSWORD_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_MRU.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_MRU, NB_COLONNE_LV[LV_REGISTRY_MRU_NB_COL]);
        strcpy(tmp,path);
        strncat(tmp,"_REG_Path.csv",MAX_PATH);
        ExportLVtoCSV(tmp, TABL_REGISTRY, LV_REGISTRY_PATH, NB_COLONNE_LV[LV_REGISTRY_PATH_NB_COL]);

        //export de la base SAM :
        strcpy(tmp,path);
        strncat(tmp,"_REG_ACCOUNT.pwdump",MAX_PATH);
        ExportLVColto(tmp, TABL_REGISTRY, LV_REGISTRY_USERS, 9);
      }
      break;
    }

    //on quitte
    EndConfig();
  }
}
//------------------------------------------------------------------------------
void EndConfig()
{
  EndDialog(Tabl[TABL_MAIN], 0);
  DeleteObject(PoliceGras);
  DeleteCriticalSection(&Sync);
}
//------------------------------------------------------------------------------
