//----------------------------------------------------------------
// Project              : Network Scanner
// Author               : Hanteville Nicolas
// Licence              : GPLv3
//----------------------------------------------------------------
/*
A faire :
  - chargementd'un csv possible !! (m�me plusieurs ?)
  - plantage lors du d�marrage sous windows64 (bug s�maphore/threads ?)
  - ajouter tests services + processus WMI
  - GetWMITests : plante
  - relifter le code pour lisibilit� !!!
  - bug du nombre d'ip

	REM NS :
    - tri 1er colonne
    - ajouter un champ de recherche
    - revoir le double cick afficher une fenetre ou le texte est s�lectionnable !
    - ajouter status bar avec nb items +  �tats des semaphore par cat�gorie pris/lib�r�s pour chaque
    - ajouter une fonction rescanne des machines non test�s (ou avec un fail)
		- bug connexion ajouter WMI + activation des services en cas d'abscence
		- ajouter check SSH
*/
/*
help : ?
http://stackoverflow.com/questions/5067240/user-logged-into-remote-machine
http://code.google.com/p/wmi-delphi-code-creator/wiki/MSCPPDevelopers
*/
//----------------------------------------------------------------
//#define _WIN32_WINNT			0x0501  //>= windows 2000
#define _WIN32_IE         0x0501  // IE5 min
//----------------------------------------------------------------
//#define DEBUG_MODE                                  1
//----------------------------------------------------------------
#include <Winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <time.h>
#include <lm.h>
#include <Winnetwk.h>
#include <iphlpapi.h>
#include <math.h>

#include "crypt/md5.h"

#define _WIN32_DCOM

#include "WbemCli.h"
#include "wbemprov.h"
#include "wbemtran.h"
#include <objbase.h>

#ifndef RESOURCES
#define RESOURCES
//----------------------------------------------------------------
#define TITLE                                       "NS v0.3.1 01/11/2013"
#define ICON_APP                                    100
//----------------------------------------------------------------
#define DEFAULT_LIST_FILES                          "conf_files.txt"
#define DEFAULT_LIST_REGISTRY                       "conf_registry.csv"
#define DEFAULT_LIST_SERVICES                       "conf_services.txt"
#define DEFAULT_LIST_SOFTWARE                       "conf_softwares.txt"
#define DEFAULT_LIST_USB                            "conf_USB.txt"
//----------------------------------------------------------------
#define LINE_SIZE                                   2048
#define MAX_LINE_SIZE                               LINE_SIZE*4
#define IP_SIZE                                     16
#define SHA256_SIZE                                 65
#define DATE_SIZE                                   26

#define ICMP_TIMEOUT                                6000
#define DIXM                                        10*1024*1024    //10mo
//----------------------------------------------------------------
#define DLG_NS                                      1000
#define GRP_DISCO                                   1001
#define ED_NET_LOGIN                                1002
#define ED_NET_PASSWORD                             1003
#define ED_NET_DOMAIN                               1004
#define CHK_NULL_SESSION                            1005
#define CHK_ALL_TEST                                1006
#define BT_LOAD_MDP_FILES                           1007

#define GRP_PERIMETER                               1015
#define IP1                                         1016
#define IP2                                         1017
#define BT_IP_CP                                    1018
#define CHK_LOAD_IP_FILE                            1019
#define BT_SAVE                                     1020

#define CB_infos                                    1031
#define LV_results                                  1032
#define CB_tests                                    1033

#define CB_IP                                       1040
#define CB_T_REGISTRY                               1041
#define CB_T_SERVICES                               1042
#define CB_T_SOFTWARE                               1043
#define CB_T_USB                                    1044
#define CB_T_FILES                                  1045

#define BT_START                                    1035
//----------------------------------------------------------------
#define SAVE_TYPE_CSV                               1
#define SAVE_TYPE_XML                               2
#define SAVE_TYPE_HTML                              3
//----------------------------------------------------------------
#define COL_IP                                      0
#define COL_DNS                                     1
#define COL_TTL                                     2
#define COL_OS                                      3
#define COL_CONFIG                                  4
#define COL_FILES                                   5
#define COL_REG                                     6
#define COL_SERVICE                                 7
#define COL_SOFTWARE                                8
#define COL_USB                                     9
#define COL_STATE                                   10

#define NB_COLUMN                                   11
//----------------------------------------------------------------
typedef struct sort_st
{
  HWND hlv;
  BOOL sort;
  unsigned int col;
}SORT_ST;
//----------------------------------------------------------------
typedef struct reg_line_st
{
  char path[LINE_SIZE];
  char value[LINE_SIZE];
  char data[LINE_SIZE];

  //format data
  BOOL data_dword;    //dword
  BOOL data_string;   //string

  //check
  BOOL check_equal;   // =
  BOOL check_inf;     // <
  BOOL check_sup;     // >
  BOOL check_diff;    // !
  BOOL check_content; // ?
  BOOL check_no_data; // *
  BOOL check_no_value;//
}REG_LINE_ST;
//----------------------------------------------------------------
typedef struct rg_st
{
  HKEY hk;
  char path[LINE_SIZE];
  char value[LINE_SIZE];
  char data[LINE_SIZE];
  unsigned short data_type;
}RG_ST;
//----------------------------------------------------------------
#define MAX_ACCOUNTS 256
typedef struct accounts_st
{
  char domain[MAX_PATH];
  char login[MAX_PATH];
  char mdp[MAX_PATH];
}ACCOUNTS_ST;

typedef struct scanne_st
{
  //BOOL disco_arp;
  BOOL disco_icmp;
  BOOL disco_dns;
  BOOL disco_netbios;

  BOOL config_service;
  BOOL config_user;
  BOOL config_software;
  BOOL config_USB;
  BOOL config_start;
  BOOL config_revers_SID;
  BOOL config_RPC;

  BOOL check_files;
  BOOL check_registry;
  BOOL check_services;
  BOOL check_software;
  BOOL check_USB;

  unsigned int nb_accounts;
  ACCOUNTS_ST accounts[MAX_ACCOUNTS];

  //use or not local account
  BOOL local_account;
  char domain[MAX_PATH];
  char login[MAX_PATH];
  char mdp[MAX_PATH];
}SCANNE_ST;
//----------------------------------------------------------------
typedef struct
{
  char c[MAX_LINE_SIZE];
}LINE_ITEM;
//----------------------------------------------------------------
BOOL scan_start, tri_order;
HANDLE h_thread_scan;

HINSTANCE hinst;
HWND h_main;
HANDLE h_log;
//----------------------------------------------------------------
//scan
#define MACH_LINUX                                  64
#define MACH_WINDOWS                                128
#define MACH_ROUTEUR                                256

//Threads
#define NB_MAX_DISCO_THREADS                        100
#define NB_MAX_NETBIOS_THREADS                      10
#define NB_MAX_FILE_THREADS                         5
#define NB_MAX_REGISTRY_THREADS                     5
#define NB_MAX_THREAD                               1000

CRITICAL_SECTION Sync;
HANDLE hs_threads,hs_disco,hs_netbios,hs_file,hs_registry;
SCANNE_ST config;
//----------------------------------------------------------------
//ICMP
typedef struct tagIPINFO
{
	unsigned char Ttl;
	unsigned char Tos;
	unsigned char Flags;
  	unsigned char OptionsSize;
  	unsigned char OptionsData;
}IPINFO;
typedef IPINFO* PIPINFO;

typedef struct tagICMPECHO
{
     DWORD Address;
     unsigned long  Status,RoundTripTime;
     unsigned char a[8];
     IPINFO  Options;
}ICMPECHO;

typedef LONG NTSTATUS;

typedef struct _IO_STATUS_BLOCK
{
  NTSTATUS Status;
  ULONG Information;
}IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef VOID WINAPI(*PIO_APC_ROUTINE)
(
  IN PVOID ApcContext,
  IN PIO_STATUS_BLOCK IoStatusBlock,
  IN ULONG Reserved
);

BOOL IcmpOk;
HANDLE hndlIcmp;
HANDLE (WINAPI *pIcmpCreateFile)(VOID);
BOOL (WINAPI *pIcmpCloseHandle)(HANDLE);
DWORD (WINAPI *pIcmpSendEcho) (HANDLE,DWORD,LPVOID,WORD, PIPINFO,    LPVOID,DWORD,DWORD);
DWORD (WINAPI *pIcmpSendEcho2) (HANDLE,HANDLE,PIO_APC_ROUTINE,PVOID,IPAddr,LPVOID,WORD,PIP_OPTION_INFORMATION,LPVOID,DWORD,DWORD);
//----------------------------------------------------------------
#endif
/*
//GUI
void init(HWND hwnd);

//LSTV
void c_Tri(HWND hlv, unsigned short colonne_ref, BOOL sort);
DWORD AddLSTVItem(char *ip, char *dns, char *ttl, char *os, char *config, char *files, char *registry, char *Services, char *software, char *USB, char *state);
BOOL SaveLSTV(HWND hlv, char *file, unsigned int type, unsigned int nb_column);
void AddLSTVUpdateItem(char *add, DWORD column, DWORD item);

//LSB
void AddMsg(HWND hwnd, char *type, char *txt, char *info);
BOOL LSBExist(DWORD lsb, char *st);

//string
unsigned long int Contient(char*data,char*chaine);
void replace_one_char(char *buffer, unsigned long int taille, char chtoreplace, char chreplace);

//Import
void load_file_list(DWORD lsb, char *file);

//tests
void md5_init(md5_state_t *pms);
void md5_append(md5_state_t *pms, const md5_byte_t *data, int nbytes);
void md5_finish(md5_state_t *pms, md5_byte_t digest[16]);
void addIPInterval(char *ip_src, char *ip_dst);
HANDLE UserConnect(char *ip,SCANNE_ST config, char*desc);
void UserDisConnect(HANDLE htoken);
int Ping(char *ip);
BOOL ResDNS(char *ip, char *name, unsigned int sz_max);
BOOL Netbios_OS(char *ip, char*txtOS, char *name, char *domain, unsigned int sz_max);
BOOL Netbios_NULLSession(char *ip);
BOOL TestReversSID(char *ip, char* user);
BOOL Netbios_Time(wchar_t *server, char *time, unsigned int sz_max);
BOOL Netbios_Share(wchar_t *server, char *share, unsigned int sz_max);

BOOL GetWMITests(DWORD iitem, char *ip, SCANNE_ST config);



//threads
DWORD WINAPI load_file_ip(LPVOID lParam);
DWORD WINAPI ScanIp(LPVOID lParam);
DWORD WINAPI scan(LPVOID lParam);

*/
