#include "pch.h"
#include "Img_Manager.h"
#include "MyPng.h"
#include <string>


CImg_Manager* CImg_Manager::m_pInstance = nullptr;

namespace
{
	const std::wstring& ResourceRoot()
	{
		static const std::wstring s_root = []() -> std::wstring
		{
			wchar_t szExe[MAX_PATH] = {};
			if (0 == GetModuleFileNameW(nullptr, szExe, MAX_PATH))
				return std::wstring();

			std::wstring dir(szExe);
			size_t cut = dir.find_last_of(L"\\/");
			if (cut == std::wstring::npos)
				return std::wstring();
			dir.resize(cut);

			for (int i = 0; i < 6; ++i)
			{
				std::wstring cand = dir + L"\\Resource";
				DWORD attr = GetFileAttributesW(cand.c_str());
				if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
					return cand;

				size_t up = dir.find_last_of(L"\\/");
				if (up == std::wstring::npos)
					break;
				dir.resize(up);
			}
			return std::wstring();
		}();
		return s_root;
	}

	std::wstring ResolveResourcePath(const TCHAR* pFilePath)
	{
		if (!pFilePath)
			return std::wstring();

		std::wstring src(pFilePath);
		const std::wstring& root = ResourceRoot();
		if (root.empty())
			return src;

		size_t pos = src.find(L"Resource");
		if (pos == std::wstring::npos)
			return src;

		pos += 8;
		while (pos < src.size() && (src[pos] == L'\\' || src[pos] == L'/'))
			++pos;

		std::wstring full = root + L"\\" + src.substr(pos);
		if (GetFileAttributesW(full.c_str()) != INVALID_FILE_ATTRIBUTES)
			return full;

		return src;
	}
}

CImg_Manager::CImg_Manager()
{
}

CImg_Manager::~CImg_Manager()
{
	Release();
}

void CImg_Manager::Insert_Png(const TCHAR* pFilePath, const TCHAR* pImgKey)
{
	auto iter = std::find_if(m_mapPng.begin(), m_mapPng.end(), CTagFinder(pImgKey));
	if (iter == m_mapPng.end())
	{
		CMyPng* pPng = new CMyPng;
		std::wstring resolved = ResolveResourcePath(pFilePath);
		pPng->Load_Png(resolved.c_str(), m_pRT);
		m_mapPng.insert({ pImgKey, pPng });
	}
}

ID2D1Bitmap* CImg_Manager::Find_Png(const TCHAR* pImgKey)
{
	auto iter = std::find_if(m_mapPng.begin(), m_mapPng.end(), CTagFinder(pImgKey));
	if (iter == m_mapPng.end())
		return nullptr;
	return iter->second->Get_Bitmap();
}
void CImg_Manager::Release(void)
{
	std::for_each(m_mapPng.begin(), m_mapPng.end(), CDeleteMap());
	m_mapPng.clear();

}
