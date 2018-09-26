//
// 2018-05-01, jjuiddong
// vworld web download *.bil, *.dds, *.dat, .. etc
//
#pragma once

#include "taskwebdownload.h"


class cQuadTileManager;

namespace gis
{

	class cVWorldWebDownloader
	{
	public:
		cVWorldWebDownloader();
		virtual ~cVWorldWebDownloader();

		bool DownloadFile(const int level, const int xLoc, const int yLoc
			, const int idx
			, const eLayerName::Enum type
			, cQuadTileManager &tileMgr
			, iDownloadFinishListener *listener
			, const char *dataFileName = NULL
		);

		void UpdateDownload();
		bool Insert(const sDownloadData &dnData);
		bool Remove(const sDownloadData &dnData);
		void Clear();


	public:
		bool m_isOfflineMode;
		CriticalSection m_cs;
		vector<sDownloadData> m_complete;
		set<iDownloadFinishListener*> m_listeners;

		struct sWebDownloader
		{
			cThread thread;
			CriticalSection cs;
			set<__int64> requestIds; // 중복 체크, key = cQuadTree::MakeKey()

			sWebDownloader(const char *name, const int maxTask, iMemoryPool3Destructor *memPool)
				: thread(name, maxTask, memPool) {}
		};
		sWebDownloader *m_webDownloader[eLayerName::MAX_LAYERNAME];
		cMemoryPool3<cTaskWebDownload, 2048> m_memPoolTask;
	};

}
