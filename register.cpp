#include <cstdio>

#include <filesystem>
#include <Windows.h>
#include <ShlObj_core.h>
#include "winrt/base.h"
#include <winrt/windows.applicationmodel.h>
#include <winrt/windows.management.h>
#include <winrt/windows.management.core.h>

#include "config.h"
#include "winrt/windows.foundation.collections.h"
#include "winrt/windows.management.deployment.h"

static HINSTANCE g_instance{};

// the dll path
std::filesystem::path CurrentPath()
{
	wchar_t path[MAX_PATH]{};
	GetModuleFileNameW(g_instance, path, std::size(path));
	return path;
}

namespace
{
	// only for debug
	const wchar_t* GetPredefineKeyName(HKEY key)
	{
#define COMPARE_KEY_NAME(KEY) if(key == KEY) return L"" #KEY;

		COMPARE_KEY_NAME(HKEY_CLASSES_ROOT)
		COMPARE_KEY_NAME(HKEY_CURRENT_USER)
		COMPARE_KEY_NAME(HKEY_LOCAL_MACHINE)
		COMPARE_KEY_NAME(HKEY_USERS)

#undef COMPARE_KEY_NAME
		return {};
	}

	struct RegKey
	{
		HKEY handle{};
		LSTATUS status{};

		RegKey(HKEY key, std::wstring_view sub_key, bool create_if_not_exist = false)
		{
			status = RegOpenKeyExW(key, sub_key.data(), 0, KEY_READ, &handle);
			if (status != ERROR_SUCCESS)
			{
				DWORD disposition = 0;
				status = RegCreateKeyExW(key, sub_key.data(), 0, nullptr, REG_OPTION_NON_VOLATILE,
				                         KEY_ALL_ACCESS, nullptr, &handle, &disposition);
				if (status == ERROR_SUCCESS)
				{
					printf("create key: %ls\\%ls\n", GetPredefineKeyName(key), sub_key.data());
				}
			}
		}

		RegKey(const RegKey&) = delete;
		RegKey& operator=(const RegKey&) = delete;

		~RegKey()
		{
			if (handle)
			{
				RegCloseKey(handle);
				handle = {};
			}
		}

		[[nodiscard]] bool Success() const { return ERROR_SUCCESS == status; }

		bool SetStringValue(std::wstring_view name, std::wstring_view value)
		{
			if (!Success())
			{
				return false;
			}
			return ERROR_SUCCESS == RegSetValueExW(handle, name.data(), 0, RRF_RT_REG_SZ,
			                                       reinterpret_cast<const BYTE*>(value.data()),
			                                       sizeof(wchar_t) * (value.size() + 1));
		}

		[[nodiscard]] std::optional<std::wstring> GetStringValue(std::wstring_view name) const
		{
			if (!Success())
			{
				return std::nullopt;
			}
			DWORD size = 0;
			if (ERROR_SUCCESS == RegGetValueW(handle, nullptr, name.data(), RRF_RT_REG_SZ, nullptr, nullptr, &size))
			{
				std::wstring r;
				r.resize(size);
				if (ERROR_SUCCESS == RegGetValueW(handle, nullptr, name.data(), RRF_RT_REG_SZ, nullptr, r.data(),
				                                  &size))
				{
					return r;
				}
			}
			return std::nullopt;
		}

		static bool Exist(HKEY key, std::wstring_view sub_key)
		{
			HKEY key_handle{};
			if (ERROR_SUCCESS == RegOpenKeyExW(key, sub_key.data(), 0, KEY_READ, &key_handle))
			{
				RegCloseKey(key_handle);
				return true;
			}
			return false;
		}

		static void DeleteKey(HKEY key, std::wstring_view sub_key)
		{
			if (!Exist(key, sub_key))
			{
				return;
			}
			if (ERROR_SUCCESS == RegDeleteTreeW(key, sub_key.data()))
			{
				printf("delete key: %ls\\%ls\n", GetPredefineKeyName(key), sub_key.data());
			}
		}
	};


	bool IsWin11()
	{
		const RegKey key{HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"};
		const auto build_number = key.GetStringValue(L"CurrentBuildNumber");
		if (build_number.has_value())
		{
			const auto version = std::stoi(build_number.value());
			if (version >= 22000)
			{
				return true;
			}
		}
		return false;
	}
}

extern "C" BOOL WINAPI DllMain(HINSTANCE instance,
                               DWORD reason,
                               LPVOID reserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		g_instance = instance;
	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return true;
}


using namespace winrt::Windows;

bool RegisterSparsePackage()
{
	const auto path = CurrentPath();
	const auto dir_name = path.parent_path();

	const auto msix_path = dir_name / "CopyPath.msix";
	if (!exists(msix_path))
	{
		printf("msix not exists:%s\n", msix_path.generic_string().c_str());
		return false;
	}

	Foundation::Uri external_uri(dir_name.generic_wstring());
	Foundation::Uri package_uri(msix_path.generic_wstring());

	Management::Deployment::AddPackageOptions add_package_options;
	add_package_options.ExternalLocationUri(external_uri);
	add_package_options.AllowUnsigned(true);
	Management::Deployment::PackageManager package_manager;
	auto deployment_operation = package_manager.AddPackageByUriAsync(package_uri, add_package_options);
	auto deploy_result = deployment_operation.get();
	if (SUCCEEDED(deploy_result.ExtendedErrorCode()))
	{
		printf("register sparse package %ls success\n", msix_path.c_str());
		return true;
	}
	else
	{
		printf("register sparse package %ls fail\n", msix_path.c_str());
		return false;
	}
}

bool UnRegisterSparsePackage()
{
	Management::Deployment::PackageManager package_manager;
	std::optional<ApplicationModel::Package> package;
	auto packages = package_manager.FindPackagesForUser({});
	for (const auto& pkg : packages)
	{
		if (pkg.Id().Name() == L"CopyPath")
		{
			package = pkg;
			break;
		}
	}
	if (!package.has_value())
	{
		return false;
	}
	winrt::hstring full_name = package.value().Id().FullName();
	auto operation = package_manager.RemovePackageAsync(full_name, Management::Deployment::RemovalOptions::None);
	auto result = operation.get();

	return SUCCEEDED(result.ExtendedErrorCode());
}

#define COMMAND_CLSID "{" COMMAND_UUID "}"

void RegisterContextMenu()
{
	RegKey shell{HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\*\\shell\\CopyPath", true};
	// check whether in admin mode
	if (!shell.Success())
	{
		printf("register key fail, try run in admin mode\n");
		return;
	}
	shell.SetStringValue(L"", L"CopyPath");
	shell.SetStringValue(L"ExplorerCommandHandler", L"" COMMAND_CLSID);
	shell.SetStringValue(L"NeverDefault", L"");

	RegKey clsid{HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\CLSID\\" COMMAND_CLSID, true};
	clsid.SetStringValue(L"", L"CopyPath");


	RegKey in_proc_server{
		HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\CLSID\\" COMMAND_CLSID "\\InProcServer32", true
	};
	in_proc_server.SetStringValue(L"", CurrentPath().generic_wstring());
	in_proc_server.SetStringValue(L"ThreadingModel", L"Apartment");
}

void UnregisterContextMenu()
{
	RegKey::DeleteKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\*\\shell\\CopyPath");
	RegKey::DeleteKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\CLSID\\" COMMAND_CLSID);
}
#undef COMMAND_CLSID


__declspec(dllexport) void Install()
{
	if (IsWin11())
	{
		printf("win11\n");
		// must run in another thread
		std::thread t{
			[]
			{
				winrt::init_apartment();
				UnRegisterSparsePackage();
				RegisterSparsePackage();
			}
		};
		t.join();
	}
	UnregisterContextMenu();
	RegisterContextMenu();
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

__declspec(dllexport) void UnInstall()
{
	if (IsWin11())
	{
		printf("win11\n");
		std::thread t{
			[]
			{
				winrt::init_apartment();
				UnRegisterSparsePackage();
			}
		};
		t.join();
	}
	UnregisterContextMenu();
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
