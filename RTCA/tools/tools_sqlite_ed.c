//------------------------------------------------------------------------------
// Projet RtCA          : Read to Catch All
// Auteur               : Nicolas Hanteville
// Site                 : http://code.google.com/p/omnia-projetcs/
// Licence              : GPL V3
//------------------------------------------------------------------------------
#include "../RtCA.h"
//------------------------------------------------------------------------------
#define TYPE_SQLITE_LOAD_TABLES         0x0001
#define TYPE_SQLITE_LOAD_TABLE_SELECTED 0x0002
#define TYPE_SQLITE_REQUEST             0x0003
//------------------------------------------------------------------------------
sqlite3 *db_sqlite_test;
DWORD nb_current_col_sqlite;
//------------------------------------------------------------------------------
int callback_sqlite_sqlite_ed(void *datas, int argc, char **argv, char **azColName)
{
  FORMAT_CALBAK_TYPE *type = datas;
  switch(type->type)
  {
    //----------------------------------------
    case TYPE_SQLITE_LOAD_TABLES:SendDlgItemMessage(h_sqlite_ed,DLG_SQL_ED_LB_TABLE,LB_ADDSTRING,0,(LPARAM)argv[0]);break;
    //----------------------------------------
    case TYPE_SQLITE_LOAD_TABLE_SELECTED:
    case TYPE_SQLITE_REQUEST:
    {
      unsigned short i;
      LVITEM lvi;
      lvi.mask = LVIF_TEXT|LVIF_PARAM;
      lvi.iSubItem = 0;
      lvi.lParam = LVM_SORTITEMS;
      lvi.pszText="";
      lvi.iItem = SendDlgItemMessage(h_sqlite_ed,DLG_SQL_ED_LV_RESPONSE,LVM_GETITEMCOUNT,0,0);

      if (lvi.iItem == 0) //first item, add headers
      {
        //size
        nb_current_col_sqlite = argc;
        RECT Rect;
        GetWindowRect(GetDlgItem(h_sqlite_ed,DLG_SQL_ED_LV_RESPONSE), &Rect);
        DWORD col_sz = ((Rect.right-Rect.left)/argc) -20;

        //create column
        LVCOLUMN lvc;
        lvc.mask                = LVCF_TEXT|LVCF_WIDTH|LVCF_FMT;
        lvc.fmt                 = LVCFMT_LEFT;
        lvc.cx                  = col_sz;

        //add text header
        for (i=0;i<argc;i++)
        {
          lvc.pszText = azColName[i];
          SendDlgItemMessage(h_sqlite_ed,DLG_SQL_ED_LV_RESPONSE,LVM_INSERTCOLUMN,(WPARAM)i, (LPARAM)&lvc);
        }
      }

      //add item
      DWORD ref_item = ListView_InsertItem(GetDlgItem(h_sqlite_ed,DLG_SQL_ED_LV_RESPONSE), &lvi);

      for (i=0;i<argc;i++)
      {
        ListView_SetItemText(GetDlgItem(h_sqlite_ed,DLG_SQL_ED_LV_RESPONSE),ref_item,i,argv[i]);
      }
    }
    break;
  }
  return 0;
}
//------------------------------------------------------------------------------
BOOL CALLBACK DialogProc_sqlite_ed(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch(message)
  {
    case WM_COMMAND:
      switch (HIWORD(wParam))
      {
        case BN_CLICKED:
          switch(LOWORD(wParam))
          {
            case DLG_SQL_ED_BT_LOAD:
            {
              OPENFILENAME ofn;
              char file[MAX_PATH]="";
              ZeroMemory(&ofn, sizeof(OPENFILENAME));
              ofn.lStructSize = sizeof(OPENFILENAME);
              ofn.hwndOwner = h_sqlite_ed;
              ofn.lpstrFile = file;
              ofn.nMaxFile = MAX_PATH;
              ofn.lpstrFilter ="SQLITE (*.sqlite)\0*.sqlite\0SQLITE (*.db)\0*.db\0ALL (*.*)\0*.*\0";
              ofn.nFilterIndex = 3;
              ofn.Flags =OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_FILEMUSTEXIST| OFN_ALLOWMULTISELECT | OFN_EXPLORER;
              ofn.lpstrDefExt ="*.sqlite\0";

              if (GetOpenFileName(&ofn)==TRUE)
              {
                //open file
                sqlite3_open(file, &db_sqlite_test);
                //clean
                SendDlgItemMessage(hwnd,DLG_SQL_ED_LB_TABLE,LB_RESETCONTENT,0,0);
                //load list of tables
                FORMAT_CALBAK_READ_INFO fcri;
                fcri.type = TYPE_SQLITE_LOAD_TABLES;
                sqlite3_exec(db_sqlite_test,"SELECT DISTINCT tbl_name FROM sqlite_master ORDER BY tbl_name;", callback_sqlite_sqlite_ed, &fcri, NULL);
                SetWindowText(hwnd,file);
              }
            }
            break;
            case DLG_SQL_ED_BT_CLOSE:
              sqlite3_close(db_sqlite_test);
              db_sqlite_test = NULL;

              //delete columns
              while (ListView_DeleteColumn(GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE),1));
              ListView_DeleteColumn(GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE),0);

              //init LV
              TRI_SQLITE_ED = FALSE;
              SendDlgItemMessage(hwnd,DLG_SQL_ED_STATE_SB,SB_SETTEXT,0, (LPARAM)"");
              SendDlgItemMessage(hwnd,DLG_SQL_ED_LV_RESPONSE,LVM_DELETEALLITEMS,0, (LPARAM)"");

              //lB
              SendDlgItemMessage(hwnd,DLG_SQL_ED_LB_TABLE,LB_RESETCONTENT,0, (LPARAM)"");
              SetWindowText(hwnd,NOM_FULL_APPLI);
            break;
            case DLG_SQL_ED_BT_SEND:
            {
              //delete columns
              while (ListView_DeleteColumn(GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE),1));
              ListView_DeleteColumn(GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE),0);

              //init LV
              TRI_SQLITE_ED = FALSE;
              SendDlgItemMessage(hwnd,DLG_SQL_ED_STATE_SB,SB_SETTEXT,0, (LPARAM)"");
              SendDlgItemMessage(hwnd,DLG_SQL_ED_LV_RESPONSE,LVM_DELETEALLITEMS,0, (LPARAM)"");

              char request[MAX_PATH]="";
              GetWindowText(GetDlgItem(hwnd,DLG_SQL_ED_ED_REQUEST),request,MAX_PATH);

              //request
              FORMAT_CALBAK_READ_INFO fcri;
              fcri.type = TYPE_SQLITE_REQUEST;
              char *error_msg = 0;
              if (sqlite3_exec(db_sqlite_test,request, callback_sqlite_sqlite_ed, &fcri, &error_msg)!= SQLITE_OK)
              {
                snprintf(request,MAX_PATH,"Error: %s",error_msg);
                SendDlgItemMessage(hwnd,DLG_SQL_ED_STATE_SB,SB_SETTEXT,0, (LPARAM)request);
                sqlite3_free(error_msg);
              }else
              {
                //set number of items
                snprintf(request,MAX_PATH,"Item(s) : %lu",SendDlgItemMessage(hwnd,DLG_SQL_ED_LV_RESPONSE,LVM_GETITEMCOUNT,0,0));
                SendDlgItemMessage(hwnd,DLG_SQL_ED_STATE_SB,SB_SETTEXT,0, (LPARAM)request);
              }
            }
            break;

            case POPUP_SQLITE_REQUEST_SELECT:SetWindowText(GetDlgItem((HWND)hwnd,DLG_SQL_ED_ED_REQUEST),"SELECT * FROM table;");break;
            case POPUP_SQLITE_REQUEST_INSERT:SetWindowText(GetDlgItem((HWND)hwnd,DLG_SQL_ED_ED_REQUEST),"INSERT INTO table (param1,param2) VALUES(\"data1\",\"data2\");");break;
            case POPUP_SQLITE_REQUEST_DELETE:SetWindowText(GetDlgItem((HWND)hwnd,DLG_SQL_ED_ED_REQUEST),"DELETE FROM table WHERE value=\"1\";");break;
            case POPUP_SQLITE_REQUEST_VACCUM:SetWindowText(GetDlgItem((HWND)hwnd,DLG_SQL_ED_ED_REQUEST),"VACUUM;");break;
            case POPUP_SQLITE_REQUEST_JOURNAL_ON:SetWindowText(GetDlgItem((HWND)hwnd,DLG_SQL_ED_ED_REQUEST),"PRAGMA journal_mode = ON;");break;
            case POPUP_SQLITE_REQUEST_JOURNAL_OFF:SetWindowText(GetDlgItem((HWND)hwnd,DLG_SQL_ED_ED_REQUEST),"PRAGMA journal_mode = OFF;");break;
            case POPUP_SQLITE_REQUEST_BEGIN_TRANSACTION:SetWindowText(GetDlgItem((HWND)hwnd,DLG_SQL_ED_ED_REQUEST),"BEGIN TRANSACTION;");break;
            case POPUP_SQLITE_REQUEST_END_TRANSACTION:SetWindowText(GetDlgItem((HWND)hwnd,DLG_SQL_ED_ED_REQUEST),"END TRANSACTION;");break;

            case DLG_SQL_ED_BT_MODELS:
            {
              HMENU hmenu;
              if ((hmenu = LoadMenu(hinst, MAKEINTRESOURCE(POPUP_SQLITE_REQUEST)))!= NULL)
              {
                RECT Rect;
                GetWindowRect(GetDlgItem((HWND)hwnd,DLG_SQL_ED_BT_MODELS), &Rect);
                TrackPopupMenuEx(GetSubMenu(hmenu, 0), 0, Rect.left, Rect.bottom,hwnd, NULL);
                DestroyMenu(hmenu);
              }
            }
            break;
            //-----------------------------------------------------
            case POPUP_S_VIEW:
            {
              char file[MAX_PATH]="sqlite_export";
              OPENFILENAME ofn;
              ZeroMemory(&ofn, sizeof(OPENFILENAME));
              ofn.lStructSize = sizeof(OPENFILENAME);
              ofn.hwndOwner = h_sqlite_ed;
              ofn.lpstrFile = file;
              ofn.nMaxFile = MAX_PATH;
              ofn.lpstrFilter ="*.csv \0*.csv\0*.xml \0*.xml\0*.html \0*.html\0";
              ofn.nFilterIndex = 1;
              ofn.Flags =OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
              ofn.lpstrDefExt =".csv\0";
              if (GetSaveFileName(&ofn)==TRUE)
              {
                SaveLSTV(GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE), file, ofn.nFilterIndex, nb_current_col_sqlite);
                SendMessage(hstatus_bar,SB_SETTEXT,0, (LPARAM)"Export done !!!");
              }
            }
            break;
            //-----------------------------------------------------
            case POPUP_S_SELECTION:
            {
              char file[MAX_PATH]="sqlite_export";
              OPENFILENAME ofn;
              ZeroMemory(&ofn, sizeof(OPENFILENAME));
              ofn.lStructSize = sizeof(OPENFILENAME);
              ofn.hwndOwner = h_sqlite_ed;
              ofn.lpstrFile = file;
              ofn.nMaxFile = MAX_PATH;
              ofn.lpstrFilter ="*.csv \0*.csv\0*.xml \0*.xml\0*.html \0*.html\0";
              ofn.nFilterIndex = 1;
              ofn.Flags =OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
              ofn.lpstrDefExt =".csv\0";
              if (GetSaveFileName(&ofn)==TRUE)
              {
                SaveLSTVSelectedItems(GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE), file, ofn.nFilterIndex, nb_current_col_sqlite);
                SendMessage(hstatus_bar,SB_SETTEXT,0, (LPARAM)"Export done !!!");
              }
            }
            break;
          }
        break;
        case CBN_SELCHANGE:
          switch(LOWORD(wParam))
          {
            case DLG_SQL_ED_LB_TABLE:
            {
              //init LV
              TRI_SQLITE_ED = FALSE;
              SendDlgItemMessage(hwnd,DLG_SQL_ED_STATE_SB,SB_SETTEXT,0, (LPARAM)"");
              SendDlgItemMessage(hwnd,DLG_SQL_ED_LV_RESPONSE,LVM_DELETEALLITEMS,0, (LPARAM)"");

              //delete columns
              while (ListView_DeleteColumn(GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE),1));
              ListView_DeleteColumn(GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE),0);

              //get actual table
              char tmp_table[MAX_PATH]="";
              DWORD id_item = SendDlgItemMessage(hwnd,DLG_SQL_ED_LB_TABLE,LB_GETCURSEL,0,0);
              if (SendDlgItemMessage(hwnd,DLG_SQL_ED_LB_TABLE,LB_GETTEXTLEN,id_item,0) <= MAX_PATH)
              {
                SendDlgItemMessage(hwnd,DLG_SQL_ED_LB_TABLE,LB_GETTEXT,id_item,(LPARAM)tmp_table);

                //request it
                char request[MAX_PATH];
                snprintf(request,MAX_PATH,"SELECT * FROM %s;",tmp_table);
                FORMAT_CALBAK_READ_INFO fcri;
                fcri.type = TYPE_SQLITE_LOAD_TABLE_SELECTED;
                sqlite3_exec(db_sqlite_test,request, callback_sqlite_sqlite_ed, &fcri, NULL);

                //set number of items
                snprintf(request,MAX_PATH,"Item(s) : %lu",SendDlgItemMessage(hwnd,DLG_SQL_ED_LV_RESPONSE,LVM_GETITEMCOUNT,0,0));
                SendDlgItemMessage(hwnd,DLG_SQL_ED_STATE_SB,SB_SETTEXT,0, (LPARAM)request);
              }
            }
            break;
          }
        break;
      }
    break;
    case WM_DROPFILES://gestion du drag and drop de fichier ^^
    {
      char file[MAX_PATH]="";
      HDROP H_DropInfo=(HDROP)wParam;
      DWORD i=0,nb_path = DragQueryFile(H_DropInfo, 0xFFFFFFFF, file, MAX_PATH);
      //only fisrt item or fisrt good item
      for (i=0;i<nb_path;i++)
      {
        file[0] = 0;
        DragQueryFile(H_DropInfo, i, file, MAX_PATH);
        if (file[0] != 0)break;
      }


      //close if already open
      sqlite3_close(db_sqlite_test);
      db_sqlite_test = NULL;

      //delete columns
      while (ListView_DeleteColumn(GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE),1));
      ListView_DeleteColumn(GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE),0);


      //init LV
      TRI_SQLITE_ED = FALSE;
      SendDlgItemMessage(hwnd,DLG_SQL_ED_STATE_SB,SB_SETTEXT,0, (LPARAM)"");
      SendDlgItemMessage(hwnd,DLG_SQL_ED_LV_RESPONSE,LVM_DELETEALLITEMS,0, (LPARAM)"");

      //open file
      sqlite3_open(file, &db_sqlite_test);
      //clean
      SendDlgItemMessage(hwnd,DLG_SQL_ED_LB_TABLE,LB_RESETCONTENT,0,0);
      //load list of tables
      FORMAT_CALBAK_READ_INFO fcri;
      fcri.type = TYPE_SQLITE_LOAD_TABLES;
      sqlite3_exec(db_sqlite_test,"SELECT DISTINCT tbl_name FROM sqlite_master ORDER BY tbl_name;", callback_sqlite_sqlite_ed, &fcri, NULL);

      DragFinish(H_DropInfo);
      SetWindowText(hwnd,file);
    }
    break;
    case WM_CLOSE :
      ShowWindow(hwnd, SW_HIDE);
    break;
    case WM_SIZE:
    {
      unsigned int mWidth  = LOWORD(lParam);
      unsigned int mHeight = HIWORD(lParam);

      //controle de la taille minimum
      if (mWidth<610 ||mHeight<300)
      {
        RECT Rect;
        GetWindowRect(hwnd, &Rect);
        MoveWindow(hwnd,Rect.left,Rect.top,600+20,300+64,TRUE);
      }else
      {
        MoveWindow(GetDlgItem(hwnd,DLG_SQL_ED_LB_TABLE)         ,2,0,192,mHeight-68,TRUE);
        MoveWindow(GetDlgItem(hwnd,DLG_SQL_ED_BT_LOAD)          ,2,mHeight-55,95,30,TRUE);
        MoveWindow(GetDlgItem(hwnd,DLG_SQL_ED_BT_CLOSE)         ,97,mHeight-55,95,30,TRUE);

        MoveWindow(GetDlgItem(hwnd,DLG_SQL_ED_ED_REQUEST)       ,200,0,mWidth-245,200,TRUE);

        MoveWindow(GetDlgItem(hwnd,DLG_SQL_ED_BT_SEND)          ,mWidth-43,0,41,160,TRUE);
        MoveWindow(GetDlgItem(hwnd,DLG_SQL_ED_BT_MODELS)          ,mWidth-43,162,41,38,TRUE);

        MoveWindow(GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE)      ,200,202,mWidth-205,mHeight-228,TRUE);
        MoveWindow(GetDlgItem(hwnd,DLG_SQL_ED_STATE_SB)         ,0,mHeight-22,mWidth,22,TRUE);
      }
      InvalidateRect(hwnd, NULL, TRUE);
    }
    break;
    case WM_NOTIFY:
      switch(((LPNMHDR)lParam)->code)
      {
        case LVN_COLUMNCLICK:
          switch(((LPNMHDR)lParam)->idFrom)
          {
            case DLG_SQL_ED_LV_RESPONSE:
              TRI_SQLITE_ED = !TRI_SQLITE_ED;
              c_Tri(GetDlgItem(hwnd,((LPNMHDR)lParam)->idFrom),((LPNMLISTVIEW)lParam)->iSubItem,TRI_SQLITE_ED);
            break;
          }
        break;
        case NM_DBLCLK:
          if (LOWORD(wParam) == DLG_SQL_ED_LV_RESPONSE)
          {
            HANDLE hlstv = GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE);
            long i, index = SendMessage(hlstv,LVM_GETNEXTITEM,-1,LVNI_FOCUSED);
            if (index > -1)
            {
              char tmp[MAX_LINE_SIZE+1], tmp2[MAX_LINE_SIZE+1];
              RichEditInit(GetDlgItem(h_info,DLG_INFO_TXT));
              LVCOLUMN lvc;
              lvc.mask        = LVCF_TEXT;
              lvc.cchTextMax  = MAX_LINE_SIZE;
              lvc.pszText     = tmp;

              for (i=0;i<nb_current_col_sqlite;i++)
              {
                tmp[0] = 0;
                tmp2[0] = 0;
                if (SendMessage(hlstv,LVM_GETCOLUMN,(WPARAM)i,(LPARAM)&lvc) != 0)
                {
                  if (strlen(tmp)>0)
                  {
                    ListView_GetItemText(hlstv,index,i,tmp2,MAX_LINE_SIZE);

                    if (strlen(tmp2)>0)
                    {
                      RichEditCouleur(GetDlgItem(h_info,DLG_INFO_TXT),NOIR,"\r\n[");
                      RichEditCouleurGras(GetDlgItem(h_info,DLG_INFO_TXT),NOIR,tmp);
                      RichEditCouleur(GetDlgItem(h_info,DLG_INFO_TXT),NOIR,"]\r\n");
                      RichEditCouleur(GetDlgItem(h_info,DLG_INFO_TXT),NOIR,tmp2);
                      RichEditCouleur(GetDlgItem(h_info,DLG_INFO_TXT),NOIR,"\r\n");
                    }
                  }
                }
              }
              RichSetTopPos(GetDlgItem(h_info,DLG_INFO_TXT));
              if(RichEditTextSize(GetDlgItem(h_info,DLG_INFO_TXT)))
              {
                ShowWindow (h_info, SW_SHOW);
                UpdateWindow(h_info);
              }
            }
            RichSetTopPos(GetDlgItem(h_info,DLG_INFO_TXT));
          }
        break;
      }
    break;
    case WM_CONTEXTMENU:
      if ((HWND)wParam == GetDlgItem(hwnd,DLG_SQL_ED_LV_RESPONSE))
      {
        HMENU hmenu;
        if ((hmenu = LoadMenu(hinst, MAKEINTRESOURCE(POPUP_LSTV_SQLITE)))!= NULL)
        {
          ModifyMenu(hmenu,POPUP_S_VIEW           ,MF_BYCOMMAND|MF_STRING ,POPUP_S_VIEW           ,cps[TXT_POPUP_S_VIEW].c);
          ModifyMenu(hmenu,POPUP_S_SELECTION      ,MF_BYCOMMAND|MF_STRING ,POPUP_S_SELECTION      ,cps[TXT_POPUP_S_SELECTION].c);

          POINT pos;
          if (GetCursorPos(&pos)!=0)
          {
            TrackPopupMenuEx(GetSubMenu(hmenu, 0), 0, pos.x, pos.y,hwnd, NULL);
          }else TrackPopupMenuEx(GetSubMenu(hmenu, 0), 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),hwnd, NULL);
          DestroyMenu(hmenu);
        }
      }
    break;
  }
  return 0;
}
