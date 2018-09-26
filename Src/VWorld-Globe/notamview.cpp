
#include "stdafx.h"
#include "notamview.h"
#include "mapview.h"


using namespace graphic;
using namespace framework;


cNotamView::cNotamView(const StrId &name)
	: framework::cDockWindow(name)
{
}

cNotamView::~cNotamView()
{
}


void cNotamView::OnUpdate(const float deltaSeconds)
{
}


void cNotamView::OnRender(const float deltaSeconds)
{
	ImGui::Text("NOTAM");
	ImGui::PushID(10);
	ImGui::InputTextMultiline("", m_notamStr.m_str, m_notamStr.SIZE
		, ImVec2(m_rect.Width(), 300));
	ImGui::PopID();

	if (ImGui::Button("Parse"))
	{
		ParseNotam(m_notamStr.c_str());
	}
}


bool cNotamView::ParseNotam(const char *str)
{
	gis::cNotam notam;
	if (!notam.Parse(str))
		return false;

	gis::notam::cAttrE &attrE = notam.m_attrE;

	if (attrE.m_isLoad)
	{
		if (gis::notam::eAttrArea::CIRCLE == attrE.m_areaType)
		{
			g_root.m_airSpace.CreateCircle(
				g_root.m_mapView->GetRenderer()
				, attrE.m_circle.m_loc
				, attrE.m_circle.m_radius / 60.f
			);

			g_root.m_airSpace.m_faceColor = cColor(1.f, 1.f, 0.f, 0.4f);
		}
		else if (gis::notam::eAttrArea::POLYGON == attrE.m_areaType)
		{
			g_root.m_airSpace.Create(
				g_root.m_mapView->GetRenderer()
				, attrE.m_polygon
			);

			g_root.m_airSpace.m_faceColor = cColor(1.f, 1.f, 0.f, 0.4f);
		}
	}

	return true;
}
