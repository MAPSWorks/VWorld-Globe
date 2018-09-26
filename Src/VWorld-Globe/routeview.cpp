
#include "stdafx.h"
#include "routeview.h"
//#include "../tinyxml\tinyxml.h"
#include "vworldglobe.h"
#include "mapview.h"

using namespace graphic;
using namespace framework;


cRouteView::cRouteView(const StrId &name)
	: framework::cDockWindow(name)
{
}

cRouteView::~cRouteView()
{
}


void cRouteView::OnUpdate(const float deltaSeconds)
{
}


void cRouteView::OnRender(const float deltaSeconds)
{
	// 마우스 클릭, 경위도 출력
	ImGui::DragFloat2("Lon, Lat", (float*)&g_root.m_lonLat, 0.00001f, 0, 0, "%.4f");
	
	static StrPath path = "route4.gpx";
	ImGui::PushID(&path);
	ImGui::InputText("", path.m_str, path.SIZE);
	ImGui::PopID();
	ImGui::SameLine();
	if (ImGui::Button("Read AirSpace file "))
	{
		cRoute route;
		if (route.Read(path.c_str()))
		{
			cMapView *mapView = ((cViewer*)g_application)->m_simView;
			g_root.m_airSpace.Create(mapView->GetRenderer(), route.m_path
				, eAirSpaceType::POLYGON);
		}
	}

	static StrPath routePath = "../media/youngwol.gpx";
	ImGui::PushID(&routePath);
	ImGui::InputText("", routePath.m_str, routePath.SIZE);
	ImGui::PopID();
	ImGui::SameLine();
	if (ImGui::Button("Read Route file"))
	{
		if (m_route.Read(routePath.c_str()))
		{
			m_route.GeneratePath(g_root.m_genRouteGap);
			g_root.m_route = m_route.m_genPath;
		}
	}

	cMapView *mapView = ((cViewer*)g_application)->m_simView;
	if (cMapView::eState::NORMAL == mapView->m_state)
	{
		if (ImGui::Button("Pick Route Range"))
		{
			mapView->m_state = cMapView::eState::PICK_RANGE;
			mapView->m_pickRange.clear();
		}
	}
	else
	{
		if (ImGui::Button("Generate Route Range"))
		{
			m_route.m_path = mapView->m_pickRange;
			m_route.GeneratePath(g_root.m_genRouteGap);
			g_root.m_route = m_route.m_genPath;
			mapView->m_state = cMapView::eState::NORMAL;
		}
	}
	
	if (ImGui::DragFloat("Gen Route Gap", &g_root.m_genRouteGap, 0.0001f, 0.0003f, 1.00f, "%.4f"))
	{
		m_route.GeneratePath(g_root.m_genRouteGap);
		g_root.m_route = m_route.m_genPath;
	}

	if (ImGui::DragFloat("Gen Route Altitude", &g_root.m_genRouteAltitude, 1.f, 1.f, 100.f))
	{
		m_route.GeneratePath(g_root.m_genRouteGap);
		g_root.m_route = m_route.m_genPath;
	}

	ImGui::Text("Route Size = %d", g_root.m_route.size());

	static bool isAutoPilot = false;
	if (ImGui::Checkbox("Auto Pilot", &isAutoPilot))
	{
		if (isAutoPilot)
		{
			mapView->m_autoPilot.Create(&mapView->m_camera, g_root.m_route, g_root.m_genRouteAltitude);
			mapView->m_autoPilot.Start();
		}
		else
		{
			mapView->m_autoPilot.Stop();
		}
	}

	ImGui::DragFloat("Velocity", &mapView->m_autoPilot.m_velocity, 1.f, 0, 1000.f);
	ImGui::DragFloat("Deviation Altitude", &mapView->m_autoPilot.m_devAltitude, 1.f, 0, 1000.f);
	ImGui::DragFloat("Cos Period", &mapView->m_autoPilot.m_cosPeriod, 0.001f, 1.f, 100.f);

	if (ImGui::Button("Read TrackPos"))
	{
		ReadTrackPos();
	}
	ImGui::DragFloat("TrackPos OffsetY", &g_root.m_trackOffsetY, 0.5f, 0, 1000.f);


}


bool cRouteView::ReadTrackPos()
{
	list<string> exts;
	exts.push_back("txt");
	list<string> files;
	common::CollectFiles(exts, "../Media/TrackPos_LittleEndian", files);
	if (files.empty())
		return false;

	g_root.m_track.clear();

	using namespace std;
	for (auto &path : files)
	{
		//const StrPath fileName = files.back().c_str();
		const StrPath fileName = path;
		//const StrPath fileName = "../Media/TrackPos_LittleEndian/TrackPos76.txt";
		ifstream ifs(fileName.c_str(), ios::binary);
		if (!ifs.is_open())
			return false;

		const __int64 size = fileName.FileSize() / 16 + 1;
		//g_root.m_track.reserve((int)size);

		while (!ifs.eof())
		{
			gis::Vector2d lonLat = { 0,0 };
			ifs.read((char*)&lonLat.y, sizeof(lonLat.y));
			ifs.read((char*)&lonLat.x, sizeof(lonLat.x));
			if ((lonLat.x == 0) && (lonLat.y == 0))
				break;

			const Vector3 globalPos = gis::WGS842Pos(lonLat);
			const Vector3 relPos = gis::GetRelationPos(globalPos);
			g_root.m_track.push_back(relPos);
		}
	}

	return true;
}
