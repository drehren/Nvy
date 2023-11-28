#include "jumplist.h"

#include <PathCch.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shlobj_core.h>

struct JumpList {
	IClassFactory *factory;
};

JumpList *JumpListInit() {
	JumpList *jl = new JumpList{};
	jl->factory = nullptr;

	HRESULT hr = CoGetClassObject(
		CLSID_ShellLink, CLSCTX_INPROC_SERVER, NULL, IID_PPV_ARGS(&jl->factory));
	if (FAILED(hr)) {
		jl->factory = nullptr;
	}
	return jl;
}

void JumpListDestroy(JumpList **pjl) {
	if (!*pjl) { return; }

	JumpList *jl = *pjl;
	if (jl->factory) {
		jl->factory->Release();
	}

	delete *pjl;
	*pjl = nullptr;
}

void JumpListAdd(JumpList *jl, mpack_node_t path_node) {
	if (!jl->factory) { return; }
	
	path_node = mpack_node_array_at(path_node, 0);

	int len = mpack_node_strlen(path_node);
	const char *buf = mpack_node_str(path_node);

	wchar_t *cpath = nullptr;
	{
		size_t wlen = MultiByteToWideChar(CP_UTF8, 0, buf, len, NULL, 0);
		wchar_t *wbuf = static_cast<wchar_t *>(malloc((wlen + 1) * sizeof(wchar_t)));

		MultiByteToWideChar(CP_UTF8, 0, buf, len, wbuf, wlen);

		wbuf[wlen] = 0;
		// Replace all forward slashes with backward ones
		for (int i = 0; i < wlen; i++) {
			if (wbuf[i] == L'/') { wbuf[i] = L'\\'; }
		}

		PathAllocCanonicalize(wbuf, 0, &cpath);
		free(wbuf);
	}

	IShellLink *link = nullptr;
	HRESULT hr = jl->factory->CreateInstance(NULL, IID_PPV_ARGS(&link));
	if (SUCCEEDED(hr)) {
		// Use IShellLink
		static wchar_t *nvy_path = nullptr;
		if (!nvy_path) { _get_wpgmptr(&nvy_path); }

		link->SetPath(nvy_path);
		link->SetArguments(cpath);
		if (!PathIsDirectory(cpath)) {
			link->SetIconLocation(nvy_path, 0);
		} else {
			// Use system folder icon
			link->SetIconLocation(L"%SystemRoot%\\System32\\imageres.dll", 3);
		}

		IPropertyStore *pstore = nullptr;
		link->QueryInterface(IID_PPV_ARGS(&pstore));
		if (pstore) {
			PROPVARIANT pval;
			InitPropVariantFromString(PathFindFileName(cpath), &pval);

			hr = pstore->SetValue(PKEY_Title, pval);
			if (SUCCEEDED(hr)) { hr = pstore->Commit(); }
			PropVariantClear(&pval);
			pstore->Release();
		}
		SHARDAPPIDINFOLINK info {
			.psl = link,
			.pszAppID = APP_ID,
		};
		SHAddToRecentDocs(SHARD_APPIDINFOLINK, &info);
		link->Release();
	} else {
		// Use IShellItem
		IShellItem *item;
		hr = SHCreateItemFromParsingName(cpath, NULL, IID_PPV_ARGS(&item));
		if (SUCCEEDED(hr)) {
			SHARDAPPIDINFO info {
				.psi = item,
				.pszAppID = APP_ID,
			};
			SHAddToRecentDocs(SHARD_APPIDINFO, &info);
			item->Release();
		}
	}
	if (FAILED(hr)) {
		SHAddToRecentDocs(SHARD_PATHW, cpath);
	}

	LocalFree(cpath);
}
