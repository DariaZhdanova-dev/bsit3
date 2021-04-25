#define _CRT_SECURE_NO_WARNINGS
#define _WIN32_DCOM
#define STRINGSIZE2(s) #s
#define STRINGSIZE(s) STRINGSIZE2(s)

#define VARIABLE_EVENT event_data

#define VARIABLE_EVENT_IP event_ip
#define VARIABLE_EVENT_ID event_id

#define ARG_VARIABLE_EVENT1 L"$(" STRINGSIZE(VARIABLE_EVENT) ")"
#define ARG_VARIABLE_EVENT2 L"$(" STRINGSIZE(VARIABLE_EVENT_ID) ")" " $(" STRINGSIZE(VARIABLE_EVENT_IP) ")"
#include <iostream>
#include <comdef.h>
#include <taskschd.h>
#include <list>
#include <WinCred.h>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "Credui.lib")

using namespace std;

void InitCom();
int EnumerateTasksInTheFolder(ITaskService*, WCHAR*);
int GetTasks(ITaskService*);
int GetActiveTasks();
int CreateTaskForSecChanging();
int CreateTaskForPingBlocking();

int main()
{
	while (1)
	{
		cout << "Enter command:" << endl;
		cout << "-> 1 - show all active tasks and their status" << endl;
		cout << "-> 2 - create task for security changing alarm" << endl;
		cout << "-> 3 - create task for ping blocking" << endl;
		cout << "-> 0 - exit" << endl << endl;
		cout << "-> ";
		unsigned int command;
		cin >> command;
		switch (command)
		{
		case 0:
			return 0;
		case 1:
			GetActiveTasks();
			break;
		case 2:
			CreateTaskForSecChanging();
			break;
		case 3:
			CreateTaskForPingBlocking();
			break;
		default:
			cout << "Incorrect command!" << endl;
		}
		cout << "------------" << endl;
	}
	return 0;
}

int g_counter = 0;
list <int> hash_list;

void InitCom()
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		printf("CoInitializeEx failed: %x\n", hr);
		exit(0);
	}
	hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, 0, NULL);
	if (FAILED(hr))
	{
		printf("CoInitializeSecurity failed: %x\n", hr);
		CoUninitialize();
		exit(0);
	}
}

int ror(unsigned int a, unsigned int n)
{
	unsigned int t1, t2;
	t1 = a >> n;
	t2 = a << (sizeof(a) * 8 - n);
	return t1 | t2;
}

unsigned int calc_hash(WCHAR* str)
{
	unsigned int hash = 0;
	for (unsigned int i = 0; i < wcslen(str); i++)
	{
		hash = ror(hash, 0x0D);
		hash += str[i];
	}
	return hash;
}

bool search_hash(int hash)
{
	list <int>::iterator iter;
	for (iter = hash_list.begin(); iter != hash_list.end(); iter++)
	{
		if (*iter == hash)
			return true;
	}
	return false;
}

int EnumerateTasksInTheFolder(ITaskService* pService, WCHAR* path)
{
	LONG numTasks = 0;
	ITaskFolder* pRootFolder = NULL;
	IRegisteredTaskCollection* pTaskCollection = NULL;
	HRESULT hr;
	hr = pService->GetFolder(_bstr_t(path), &pRootFolder);
	if (FAILED(hr))
	{
		printf("Cannot get Root Folder pointer: %x", hr);
		return 0;
	}
	hr = pRootFolder->GetTasks(TASK_ENUM_HIDDEN, &pTaskCollection);
	pRootFolder->Release();
	if (FAILED(hr))
	{
		printf("Cannot get the registered tasks.: %x", hr);
		return 0;
	}
	hr = pTaskCollection->get_Count(&numTasks);
	if (numTasks == 0)
	{
		printf(" No Tasks are currently running\n");
		pTaskCollection->Release();
		return 0;
	}
	for (LONG i = 0; i < numTasks; i++)
	{
		IRegisteredTask* pRegisteredTask = NULL;
		hr = pTaskCollection->get_Item(_variant_t(i + 1), &pRegisteredTask);
		if (SUCCEEDED(hr))
		{
			BSTR taskName = NULL;
			hr = pRegisteredTask->get_Name(&taskName);
			if (SUCCEEDED(hr))
			{
				int hash = calc_hash(taskName);
				if (!search_hash(hash))
				{
					hash_list.push_back(hash);

					VARIANT_BOOL Enabled;
					TASK_STATE state;

					pRegisteredTask->get_Enabled(&Enabled);
					pRegisteredTask->get_State(&state);

					if (Enabled != 0)
					{
						printf("%d. \tname  : %S\n", g_counter + 1, taskName);
						switch (state)
						{
						case TASK_STATE_UNKNOWN:
							printf("\tstate : TASK_STATE_UNKNOWN\n"); break;
						case TASK_STATE_DISABLED:
							printf("\tstate : TASK_STATE_DISABLED\n"); break;
						case TASK_STATE_QUEUED:
							printf("\tstate : TASK_STATE_QUEUED\n"); break;
						case TASK_STATE_READY:
							printf("\tstate : TASK_STATE_READY\n"); break;
						case TASK_STATE_RUNNING:
							printf("\tstate : TASK_STATE_RUNNING\n"); break;
						default:
							break;
						}
						g_counter++;
					}
				}
				SysFreeString(taskName);
			}
			else
			{
				printf("Cannot get the registered task name: %x\n", hr);
			}
			pRegisteredTask->Release();
		}
		else
		{
			printf("Cannot get the registered task item at index=%d: %x\n", i + 1, hr);
		}
	}
	pTaskCollection->Release();
	return 1;
}

int GetTasks(ITaskService* pService)
{
	HKEY hKey;
	DWORD i, k, retCode, maxlen = MAX_PATH;
	CHAR achKey[MAX_PATH];
	FILETIME ftLastWriteTime;
	retCode = RegOpenKeyExA(HKEY_LOCAL_MACHINE,	"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Schedule\\TaskCache\\Tasks", NULL, KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_64KEY, &hKey );
	if (retCode != ERROR_SUCCESS) 
	{
		cout << "Error RegOpenKeyEx " << GetLastError() << endl;
		return 0;
	}
	for (i = 0, retCode = ERROR_SUCCESS; retCode == ERROR_SUCCESS; i++)
	{
		retCode = RegEnumKeyExA(hKey, i, achKey, &maxlen, NULL, NULL, NULL, &ftLastWriteTime);
		maxlen = MAX_PATH;
		if (retCode == ERROR_SUCCESS)
		{
			DWORD dwType;
			CHAR pvData[260] = { 0 };
			DWORD dwData;
			int res = RegGetValueA(hKey, achKey, "Path", RRF_RT_REG_SZ, &dwType, pvData, &dwData);
			if (res == ERROR_SUCCESS)
			{
				for (k = strlen(pvData) - 1; pvData[k] != '\\'; k--)
				{
					pvData[k] = 0;
				}
				if (strlen(pvData) > 1)
				{
					pvData[k] = 0;
				}
				WCHAR buffer[260] = { 0 };
				mbstowcs(buffer, pvData, strlen(pvData));
				EnumerateTasksInTheFolder(pService, buffer);
			}
		}
	}
	RegCloseKey(hKey);
	return 1;
}

int GetActiveTasks()
{
	HRESULT hr;
	ITaskService* pService = NULL;
	InitCom();
	hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
	if (FAILED(hr))
	{
		printf("Failed to CoCreate an instance of the TaskService class: %x", hr);
		CoUninitialize();
		return 1;
	}
	hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
	if (FAILED(hr))
	{
		printf("ITaskService::Connect failed: %x", hr);
		pService->Release();
		CoUninitialize();
		return 1;
	}
	int res = GetTasks(pService);
	if (res == 0)
	{
		CoUninitialize();
		return 1;
	}
	pService->Release();
	CoUninitialize();
	return 0;
}

void AddTriggerToTask(ITaskFolder* pRootFolder, ITaskDefinition* pTask, const wchar_t* trigger_id, const wchar_t* description)
{
	HRESULT hr;
	ITriggerCollection* pTriggerCollection = NULL;
	hr = pTask->get_Triggers(&pTriggerCollection);
	if (FAILED(hr))
	{
		printf("\n Cannot get trigger collection: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return;
	}
	ITrigger* pTrigger = NULL;
	hr = pTriggerCollection->Create(TASK_TRIGGER_EVENT, &pTrigger);
	pTriggerCollection->Release();
	if (FAILED(hr))
	{
		printf("\n Cannot create the trigger: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return;
	}
	IEventTrigger* pEventTrigger = NULL;
	hr = pTrigger->QueryInterface(IID_IEventTrigger, (void**)&pEventTrigger);
	pTrigger->Release();
	if (FAILED(hr))
	{
		printf("\n QueryInterface call on IEventTrigger failed: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return;
	}
	hr = pEventTrigger->put_Id(_bstr_t(trigger_id));
	if (FAILED(hr))
		printf("\n Cannot put trigger ID: %x", hr);
	hr = pEventTrigger->put_Subscription((BSTR)description);
	pEventTrigger->Release();
	if (FAILED(hr))
	{
		printf("\n Cannot put the event query: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return;
	}
}

void CreateNewTask(ITaskService* pService, LPCWSTR wszTaskName, ITaskFolder** pRootFolder, ITaskDefinition** pTask)
{
	HRESULT hr;
	hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
	if (FAILED(hr))
	{
		printf(" Failed to create an instance of ITaskService: %x", hr);
		CoUninitialize();
		exit(0);
	}
	hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
	if (FAILED(hr))
	{
		printf("ITaskService::Connect failed: %x", hr);
		pService->Release();
		CoUninitialize();
		exit(0);
	}
	hr = pService->GetFolder(_bstr_t(L"\\"), pRootFolder);
	if (FAILED(hr))
	{
		printf("Cannot get Root Folder pointer: %x", hr);
		pService->Release();
		CoUninitialize();
		exit(0);
	}
	(*pRootFolder)->DeleteTask(_bstr_t(wszTaskName), 0);
	hr = pService->NewTask(0, pTask);
	pService->Release();
	if (FAILED(hr))
	{
		printf("Failed to create a task definition: %x", hr);
		(*pRootFolder)->Release();
		CoUninitialize();
		exit(0);
	}
	return;
}

void SetSomeSettings(ITaskDefinition* pTask, ITaskFolder* pRootFolder)
{
	HRESULT hr;
	if (pTask == NULL)
	{
		printf("pTask is null pointer\n");
		exit(0);
	}
	IRegistrationInfo* pRegInfo = NULL;
	hr = pTask->get_RegistrationInfo(&pRegInfo);
	if (FAILED(hr))
	{
		printf("\nCannot get identification pointer: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		exit(0);
	}
	_bstr_t bstrt("zhdanova");
	hr = pRegInfo->put_Author(bstrt);
	pRegInfo->Release();
	if (FAILED(hr))
	{
		printf("\nCannot put identification info: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		exit(0);
	}
	ITaskSettings* pSettings = NULL;
	hr = pTask->get_Settings(&pSettings);
	if (FAILED(hr))
	{
		printf("\nCannot get settings pointer: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		exit(0);
	}
	hr = pSettings->put_StartWhenAvailable(VARIANT_TRUE);
	pSettings->Release();
	if (FAILED(hr))
	{
		printf("\nCannot put setting info: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		exit(0);
	}
	return;
}

void CreateUserInterface()
{
	TCHAR pszName[CREDUI_MAX_USERNAME_LENGTH] = L"";
	TCHAR pszPwd[CREDUI_MAX_PASSWORD_LENGTH] = L"";
	CREDUI_INFO cui;
	BOOL fSave;
	DWORD dwErr;
	cui.cbSize = sizeof(CREDUI_INFO);
	cui.hwndParent = NULL;
	cui.pszMessageText = TEXT("Account information for task registration:");
	cui.pszCaptionText = TEXT("Enter Account Information for Task Registration");
	cui.hbmBanner = NULL;
	fSave = FALSE;
	dwErr = CredUIPromptForCredentials(
		&cui,								//  CREDUI_INFO structure
		TEXT(""),							//  Target for credentials
		NULL,								//  Reserved
		0,									//  Reason
		pszName,							//  User name
		CREDUI_MAX_USERNAME_LENGTH,			//  Max number for user name
		pszPwd,								//  Password
		CREDUI_MAX_PASSWORD_LENGTH,			//  Max number for password
		&fSave,								//  State of save check box
		CREDUI_FLAGS_GENERIC_CREDENTIALS |	//  Flags
		CREDUI_FLAGS_ALWAYS_SHOW_UI |
		CREDUI_FLAGS_DO_NOT_PERSIST);

	if (dwErr)
	{
		cout << "Did not get credentials." << endl;
		CoUninitialize();
		exit(0);
	}
	return;
}

int CreateTaskForSecChanging()
{
	HRESULT hr;
	InitCom();
	LPCWSTR wszTaskName = L"TASK_FOR_SECURITY_AND_EASY_TO_FIND";
	ITaskService* pService = NULL;
	ITaskFolder* pRootFolder = NULL;
	ITaskDefinition* pTask = NULL;
	CreateNewTask(pService, wszTaskName, &pRootFolder, &pTask);
	SetSomeSettings(pTask, pRootFolder);
	AddTriggerToTask(pRootFolder, pTask, L"Trigger1", L"<QueryList><Query Id='0'><Select Path='Microsoft-Windows-Windows Firewall With Advanced Security/Firewall'>*[System[Provider[@Name='Microsoft-Windows-Windows Firewall With Advanced Security'] and EventID=2003]]</Select></Query></QueryList>");
	AddTriggerToTask(pRootFolder, pTask, L"Trigger2", L"<QueryList><Query Id='0'><Select Path='Microsoft-Windows-Windows Defender/WHC'>*[System[Provider[@Name='Microsoft-Windows-Windows Defender'] and EventID=5007]]</Select></Query></QueryList>");
	AddTriggerToTask(pRootFolder, pTask, L"Trigger3", L"<QueryList><Query Id='0'><Select Path='Microsoft-Windows-Windows Defender/Operational'>*[System[Provider[@Name='Microsoft-Windows-Windows Defender'] and EventID=5007]]</Select></Query></QueryList>");
	IActionCollection* pActionCollection = NULL;
	hr = pTask->get_Actions(&pActionCollection);
	if (FAILED(hr))
	{
		printf("\n Cannot get Task collection pointer: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}
	IAction* pAction = NULL;
	hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
	pActionCollection->Release();
	if (FAILED(hr))
	{
		printf("\n Cannot create the action: %x \n", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}
	IExecAction* pExecAction = NULL;
	hr = pAction->QueryInterface(IID_IExecAction, (void**)&pExecAction);
	pAction->Release();
	if (FAILED(hr))
	{
		printf("\n QueryInterface call failed on IExecAction: %x \n", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}
	hr = pExecAction->put_Path(_bstr_t(L"C:\\MessageBox.exe"));
	if (FAILED(hr))
	{
		printf("\n Cannot add path for executable action: %x \n", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}
	pExecAction->Release();
	CreateUserInterface();
	IRegisteredTask* pRegisteredTask = NULL;
	hr = pRootFolder->RegisterTaskDefinition(_bstr_t(wszTaskName), pTask, TASK_CREATE_OR_UPDATE, _variant_t(L""), _variant_t(L""),	TASK_LOGON_INTERACTIVE_TOKEN, _variant_t(L""), &pRegisteredTask);
	if (FAILED(hr))
	{
		printf("\n Error saving the Task : %x \n", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}
	printf("Task succesfully registered. \n");
	pRootFolder->Release();
	pTask->Release();
	pRegisteredTask->Release();
	CoUninitialize();
	return 1;
}

int CreateTaskForPingBlocking()
{
	HRESULT hr;

	InitCom();

	//создание имени дл€ задачи
	LPCWSTR wszTaskName = L"TASK_FOR_PING_BLOCK";;

	ITaskService* pService = NULL;
	ITaskFolder* pRootFolder = NULL;
	ITaskDefinition* pTask = NULL;

	//создание экземпл€ра службы задач
	CreateNewTask(pService, wszTaskName, &pRootFolder, &pTask);

	//установить настройки
	SetSomeSettings(pTask, pRootFolder);

	//получение коллекции триггеров дл€ вставки триггера событи€
	ITriggerCollection* pTriggerCollection = NULL;
	hr = pTask->get_Triggers(&pTriggerCollection);
	if (FAILED(hr))
	{
		printf("\n# Cannot get trigger collection: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}

	ITrigger* pTrigger = NULL;
	hr = pTriggerCollection->Create(TASK_TRIGGER_EVENT, &pTrigger);
	pTriggerCollection->Release();
	if (FAILED(hr))
	{
		printf("\n Cannot create the trigger: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}

	IEventTrigger* pEventTrigger = NULL;
	hr = pTrigger->QueryInterface(IID_IEventTrigger, (void**)&pEventTrigger);
	pTrigger->Release();
	if (FAILED(hr))
	{
		printf("\n# QueryInterface call on IEventTrigger failed: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}

	hr = pEventTrigger->put_Id(_bstr_t(L"Trigger1"));
	if (FAILED(hr))
		printf("\n# Cannot put trigger ID: %x", hr);

	hr = pEventTrigger->put_Subscription(
		_bstr_t(L"<QueryList><Query Id='0' Path='Security'><Select Path='Security'>*[System[Provider[@Name='Microsoft-Windows-Security-Auditing'] and EventID=5152]]</Select></Query></QueryList>"));
	ITaskNamedValueCollection* pNamedValueQueries = NULL;
	hr = pEventTrigger->get_ValueQueries(&pNamedValueQueries);
	if (FAILED(hr))
	{
		printf("\n Cannot put the event collection: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}

	ITaskNamedValuePair* pNamedValuePair = NULL;
	hr = pNamedValueQueries->Create(_bstr_t(L"eventData"), _bstr_t(L"Event/EventData/Data[@Name='SourceAddress']"), &pNamedValuePair);
	pNamedValueQueries->Release();
	pNamedValuePair->Release();
	if (FAILED(hr))
	{
		printf("\n Cannot create name value pair: %x", hr);
		pEventTrigger->Release();
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}

	pEventTrigger->Release();
	if (FAILED(hr))
	{
		printf("\n# Cannot put the event query: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}

	//добавление действи€ к задаче  
	IActionCollection* pActionCollection = NULL;

	//получить указатель коллекции действий задачи
	hr = pTask->get_Actions(&pActionCollection);
	if (FAILED(hr))
	{
		printf("\n# Cannot get Task collection pointer: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}

	//создать действие, указав, что оно €вл€етс€ исполн€емым действием
	IAction* pAction = NULL;
	hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
	pActionCollection->Release();
	if (FAILED(hr))
	{
		printf("\n# Cannot create the action: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}

	IExecAction* pExecAction = NULL;
	//QI дл€ указател€ исполн€емой задачи
	hr = pAction->QueryInterface(
		IID_IExecAction, (void**)&pExecAction);
	pAction->Release();
	if (FAILED(hr))
	{
		printf("\n# QueryInterface call failed on IExecAction: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}

	//задать путь к исполн€емому файлу
	hr = pExecAction->put_Path(_bstr_t(L"C:\\MessageBox.exe"));
	if (FAILED(hr))
	{
		printf("\n# Cannot add path for executable action: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}

	hr = pExecAction->put_Arguments(_bstr_t(L"$(eventData)"));
	if (FAILED(hr))
	{
		printf("\n# Cannot add path for executable action: %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}

	pExecAction->Release();

	//безопасно получить им€ пользовател€ и пароль, задача будет
	//создать дл€ запуска с учетными данными из предоставленного
	//им€ пользовател€ и пароль
	CreateUserInterface();

	//сохранение задачи в корневой папке
	IRegisteredTask* pRegisteredTask = NULL;
	hr = pRootFolder->RegisterTaskDefinition(
		_bstr_t(wszTaskName),
		pTask,
		TASK_CREATE_OR_UPDATE,
		_variant_t(L""),
		_variant_t(L""),
		TASK_LOGON_INTERACTIVE_TOKEN,
		_variant_t(L""),
		&pRegisteredTask);
	if (FAILED(hr))
	{
		printf("\n# Error saving the Task : %x", hr);
		pRootFolder->Release();
		pTask->Release();
		CoUninitialize();
		return 0;
	}

	printf("# Task succesfully registered. \n");

	//очистка
	pRootFolder->Release();
	pTask->Release();
	pRegisteredTask->Release();
	CoUninitialize();
	return 1;
}


