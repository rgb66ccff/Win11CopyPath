#include <filesystem>

#include "config.h"
#include <ShObjIdl.h>
#include <winrt/base.h>
#include <shlwapi.h>
#include <wrl/module.h>
#include <wrl/implements.h>

#pragma comment(lib, "Shlwapi")
#pragma comment(lib, "RuntimeObject")

using namespace Microsoft::WRL;

constexpr size_t str_max_length = 4096;

template <class F>
struct Scope
{
	F f;
	~Scope() { f(); }
};

std::filesystem::path CurrentPath();

static_assert(std::size(COMMAND_UUID) == 37, "fill up the uuid in cmake");
namespace
{
	struct __declspec(uuid(COMMAND_UUID)) Command : public RuntimeClass<
			RuntimeClassFlags<ClassicCom | InhibitRoOriginateError>, IExplorerCommand>
	{
	public:
		virtual IFACEMETHODIMP GetTitle(
			/* [unique][in] */ __RPC__in_opt IShellItemArray* psiItemArray,
			                   /* [string][out] */ __RPC__deref_out_opt_string LPWSTR* ppszName) override
		{
			return SHStrDupW(L"" COMMAND_NAME, ppszName);
		}

		// get icon path
		virtual IFACEMETHODIMP GetIcon(
			/* [unique][in] */ __RPC__in_opt IShellItemArray* psiItemArray,
			                   /* [string][out] */ __RPC__deref_out_opt_string LPWSTR* ppszIcon) override
		{
			*ppszIcon = nullptr;
			const auto icon_path = CurrentPath().parent_path() / "Assets/icon.ico";
			if(!exists(icon_path))
			{
				return E_NOTIMPL;
			}
			return SHStrDupW(icon_path.generic_wstring().c_str(), ppszIcon);
		}

		virtual IFACEMETHODIMP GetToolTip(
			/* [unique][in] */ __RPC__in_opt IShellItemArray* psiItemArray,
			                   /* [string][out] */ __RPC__deref_out_opt_string LPWSTR* ppszInfotip) override
		{
			return E_NOTIMPL;
		}

		virtual IFACEMETHODIMP GetCanonicalName(
			/* [out] */ __RPC__out GUID* pguidCommandName) override
		{
			*pguidCommandName = GUID_NULL;
			return S_OK;
		}

		virtual IFACEMETHODIMP GetState(
			/* [unique][in] */ __RPC__in_opt IShellItemArray* psiItemArray,
			                   /* [in] */ BOOL fOkToBeSlow,
			                   /* [out] */ __RPC__out EXPCMDSTATE* pCmdState) override
		{
			*pCmdState = ECS_ENABLED;
			return S_OK;
		}

		virtual IFACEMETHODIMP Invoke(
			/* [unique][in] */ __RPC__in_opt IShellItemArray* psiItemArray,
			                   /* [unique][in] */ __RPC__in_opt IBindCtx* pbc) override
		{
			if (psiItemArray)
			{
				DWORD count = 0;
				std::wstring str;
				if (SUCCEEDED(psiItemArray->GetCount(&count)))
				{
					for (DWORD i = 0; i < count; ++i)
					{
						ComPtr<IShellItem> item;
						if (SUCCEEDED(psiItemArray->GetItemAt(i, &item)))
						{
							wchar_t* name = nullptr;
							if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name)))
							{
								Scope scope{
									[&]
									{
										CoTaskMemFree(name);
									}
								};

								const auto name_length = wcslen(name);
								// simply drop remaining files if exceed max length, including '\0'
								if (name_length + str.size() + 1 > str_max_length)
								{
									break;
								}
								if (i)
								{
									str += L',';
								}
								str += name;
							}
						}
					}
				}
				for (auto&& c : str)
				{
					if (c == '\\')
					{
						c = '/';
					}
				}
				if (OpenClipboard(nullptr))
				{
					Scope scope{&CloseClipboard};
					if (!EmptyClipboard())
					{
						return S_OK;
					}
					const auto char_count = (str.size() + 1);
					const auto mem_size = char_count * sizeof(wchar_t);
					auto mem = GlobalAlloc(GMEM_MOVEABLE, mem_size);
					if (mem)
					{
						auto ptr = static_cast<wchar_t*>(GlobalLock(mem));
						if (ptr)
						{
							wcscpy_s(ptr, char_count, str.c_str());
							GlobalUnlock(ptr);
							SetClipboardData(CF_UNICODETEXT, mem);
						}
						else
						{
							GlobalFree(mem);
						}
					}
				}
			}
			return S_OK;
		}

		virtual IFACEMETHODIMP GetFlags(
			/* [out] */ __RPC__out EXPCMDFLAGS* pFlags) override
		{
			*pFlags = ECF_DEFAULT;
			return S_OK;
		}

		virtual IFACEMETHODIMP EnumSubCommands(
			/* [out] */ __RPC__deref_out_opt IEnumExplorerCommand** ppEnum) override
		{
			*ppEnum = nullptr;
			return E_NOTIMPL;
		}
	};
}


CoCreatableClass(Command)
CoCreatableClassWrlCreatorMapInclude(Command)

_Check_return_
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	if (ppv == nullptr)
		return E_POINTER;
	*ppv = nullptr;
	return Module<ModuleType::InProc>::GetModule().GetClassObject(rclsid, riid, ppv);
}

__control_entrypoint(DllExport)
STDAPI DllCanUnloadNow(void)
{
	return Module<ModuleType::InProc>::GetModule().GetObjectCount() == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetActivationFactory(HSTRING activatableClassId,
                               IActivationFactory** factory)
{
	return Module<ModuleType::InProc>::GetModule().GetActivationFactory(activatableClassId, factory);
}
