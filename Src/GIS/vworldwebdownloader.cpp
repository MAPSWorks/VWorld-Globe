
#include "stdafx.h"
#include "vworldwebdownloader.h"
#include "quadtilemanager.h"

using namespace gis;


cVWorldWebDownloader::cVWorldWebDownloader()
	: m_isOfflineMode(false)
{
	for (int i = 0; i < eLayerName::MAX_LAYERNAME; ++i)
		m_webDownloader[i] = new sWebDownloader("thread", 1000, &m_memPoolTask);
}

cVWorldWebDownloader::~cVWorldWebDownloader()
{
	Clear();
}


bool cVWorldWebDownloader::DownloadFile(const int level, const int xLoc, const int yLoc
	, const int idx
	, const eLayerName::Enum type
	, cQuadTileManager &tileMgr
	, iDownloadFinishListener *listener
	, const char *dataFileName //= NULL
)
{
	if (m_isOfflineMode)
		return false;

	{
		m_listeners.insert(listener);

		if (m_webDownloader[type])
		{
			AutoCSLock cs(m_webDownloader[type]->cs);
			const __int64 key = cQuadTree<sQuadData>::MakeKey(level, xLoc, yLoc);
			if (m_webDownloader[type]->requestIds.find(key)
				!= m_webDownloader[type]->requestIds.end())
				return false; // already request
			m_webDownloader[type]->requestIds.insert(key);
		}
	}

	sDownloadData data;
	ZeroMemory(&data, sizeof(data));
	data.level = level;
	data.xLoc = xLoc;
	data.yLoc = yLoc;
	data.idx = idx;
	data.layer = type;
	data.dataFile = dataFileName;
	data.listener = listener;

	//m_webDownloader[type]->thread.AddTask(new cTaskWebDownload(this, &tileMgr, data));
	if (m_webDownloader[type])
	{
		cTaskWebDownload *taskWebDownload = m_memPoolTask.Alloc();
		taskWebDownload->SetParameter(this, &tileMgr, data);
		m_webDownloader[type]->thread.AddTask(taskWebDownload);
		if (!m_webDownloader[type]->thread.IsRun())
			m_webDownloader[type]->thread.Start();
	}

	return true;
}


// 다운로드 받은 파일을, 리스너에게 알린다.
void cVWorldWebDownloader::UpdateDownload()
{
	AutoCSLock cs(m_cs);

	for (auto &file : m_complete)
	{
		// 모든 리스너에게 알린다.
		for (auto &listener : m_listeners)
			listener->OnDownloadComplete(file);

		const __int64 key = cQuadTree<sQuadData>::MakeKey(file.level, file.xLoc, file.yLoc);
		if (m_webDownloader[file.layer])
		{
			AutoCSLock cs(m_webDownloader[file.layer]->cs);
			m_webDownloader[file.layer]->requestIds.erase(key);
		}
	}

	m_complete.clear();
}


bool cVWorldWebDownloader::Insert(const sDownloadData &dnData)
{
	AutoCSLock cs(m_cs);
	m_complete.push_back(dnData);
	return true;
}


bool cVWorldWebDownloader::Remove(const sDownloadData &dnData)
{
	RETV(!m_webDownloader[dnData.layer], false);

	const __int64 key = cQuadTree<sQuadData>::MakeKey(dnData.level, dnData.xLoc, dnData.yLoc);
	{
		AutoCSLock cs(m_webDownloader[dnData.layer]->cs);
		m_webDownloader[dnData.layer]->requestIds.erase(key);
	}
	return true;
}


void cVWorldWebDownloader::Clear()
{
	for (int i = 0; i < eLayerName::MAX_LAYERNAME; ++i)
	{
		if (m_webDownloader[i])
		{
			m_webDownloader[i]->thread.Clear();
			SAFE_DELETE(m_webDownloader[i]);
		}
	}

	{
		AutoCSLock cs(m_cs);
		m_complete.clear();
	}

	m_memPoolTask.Clear();
}
