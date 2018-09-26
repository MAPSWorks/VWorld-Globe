
#include "stdafx.h"
#include "mapview.h"

using namespace graphic;
using namespace framework;


cMapView::cMapView(const string &name) 
	: framework::cDockWindow(name)
	, m_groundPlane1(Vector3(0, 1, 0), 0)
	, m_groundPlane2(Vector3(0, -1, 0), 0)
	, m_showGround(false)
	, m_showWireframe(false)
{
}

cMapView::~cMapView() 
{
	m_quadTree.Clear();
}


bool cMapView::Init(cRenderer &renderer) 
{
	//const Vector3 eyePos(331843.563f, 478027.719f, 55058.8672f);
	//const Vector3 lookAt(331004.031f, 0.000000000f, 157745.281f);
	const Vector3 eyePos(0.f, 300000.f, -300000.f);
	const Vector3 lookAt(0.f, 0.f, 0.f);
	m_camera.SetCamera(eyePos, lookAt, Vector3(0, 1, 0));
	m_camera.SetProjection(MATH_PI / 4.f, m_rect.Width() / m_rect.Height(), 1.f, 1000000.f);
	m_camera.SetViewPort(m_rect.Width(), m_rect.Height());
	m_camera.m_isKeepHorizontal = false;

	sf::Vector2u size((u_int)m_rect.Width() - 15, (u_int)m_rect.Height() - 50);
	cViewport vp = renderer.m_viewPort;
	vp.m_vp.Width = (float)size.x;
	vp.m_vp.Height = (float)size.y;
	m_renderTarget.Create(renderer, vp, DXGI_FORMAT_R8G8B8A8_UNORM, true, true
		, DXGI_FORMAT_D24_UNORM_S8_UINT);

	m_ground.Create(renderer, 100, 100, 10, 10);

	if (!m_quadTree.Create(renderer, true))
		return false;

	m_quadTree.m_isShowDistribute = false;
	m_quadTree.m_isShowQuadTree = true;

	m_skybox.Create(renderer, "../media/skybox/sky.dds");
	m_skybox.m_isDepthNone = true;

	return true;
}


void cMapView::OnUpdate(const float deltaSeconds)
{
	// 카메라가 가르키는 방향의 경위도를 구한다.
	const gis::Vector2d camLonLat = m_quadTree.GetLongLat(m_camera.GetRay());

	m_quadTree.Update(GetRenderer(), camLonLat, deltaSeconds);
	m_camera.Update(deltaSeconds);
}


void cMapView::OnPreRender(const float deltaSeconds)
{
	cRenderer &renderer = GetRenderer();
	cAutoCam cam(&m_camera);

	renderer.UnbindTextureAll();

	GetMainCamera().Bind(renderer);
	GetMainLight().Bind(renderer);

	if (m_renderTarget.Begin(renderer))
	{
		CommonStates states(renderer.GetDevice());

		if (m_showWireframe)
		{
			renderer.GetDevContext()->RSSetState(states.Wireframe());
		}
		else
		{
			renderer.GetDevContext()->RSSetState(states.CullCounterClockwise());
			m_skybox.Render(renderer);
		}

		if (m_showGround)
			m_ground.Render(renderer);

		const Ray ray1 = GetMainCamera().GetRay();
		const Ray ray2 = GetMainCamera().GetRay(m_mousePos.x, m_mousePos.y);

		cFrustum frustum;
		frustum.SetFrustum(GetMainCamera().GetViewProjectionMatrix());
		m_quadTree.Render(renderer, deltaSeconds, frustum, ray1, ray2);

		// latLong position
		{
			const Vector3 p0 = m_quadTree.Get3DPos({ (double)g_root.m_lonLat.x, (double)g_root.m_lonLat.y });
			renderer.m_dbgLine.SetLine(p0, p0 + Vector3(0, 10, 0), 0.01f);
			renderer.m_dbgLine.Render(renderer);
		}

		// render autopilot route
		if (!g_root.m_route.empty())
		{
			for (int i=0; i < (int)g_root.m_route.size()-1; ++i)
			{
				auto &lonLat0 = g_root.m_route[i];
				auto &lonLat1 = g_root.m_route[i+1];

				const Vector3 p0 = gis::GetRelationPos(gis::WGS842Pos(lonLat0));
				const Vector3 p1 = gis::GetRelationPos(gis::WGS842Pos(lonLat1));
				renderer.m_dbgLine.SetLine(p0+ Vector3(0, 20, 0), p1 + Vector3(0, 20, 0), 0.03f);
				renderer.m_dbgLine.Render(renderer);
			}
		}

		// render track pos
		if (!g_root.m_track.empty())
		{
			for (int i = 0; i < (int)g_root.m_track.size() - 1; ++i)
			{
				auto &p0 = g_root.m_track[i];
				auto &p1 = g_root.m_track[i + 1];
				if (p0.Distance(p1) > 100.f)
					continue;

				const float h1 = m_quadTree.GetHeight({ p0.x, p0.z });
				const float h2 = m_quadTree.GetHeight({ p1.x, p1.z });
				renderer.m_dbgLine.SetLine(p0 + Vector3(0, h1+g_root.m_trackOffsetY, 0)
					, p1 + Vector3(0, h2+ g_root.m_trackOffsetY, 0), 0.03f);
				renderer.m_dbgLine.Render(renderer);
			}
		}
	}
	m_renderTarget.End(renderer);
}


void cMapView::OnRender(const float deltaSeconds) 
{
	ImVec2 pos = ImGui::GetCursorScreenPos();
	m_viewPos = { (int)(pos.x), (int)(pos.y) };
	m_viewRect = { pos.x + 5, pos.y, pos.x + m_rect.Width() - 30, pos.y + m_rect.Height() - 50 };
	ImGui::Image(m_renderTarget.m_resolvedSRV, ImVec2(m_rect.Width() - 15, m_rect.Height() - 50));

	// HUD
	const float windowAlpha = 0.0f;
	bool isOpen = true;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
	ImGui::SetNextWindowPos(pos);
	ImGui::SetNextWindowBgAlpha(windowAlpha);
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(min(m_viewRect.Width(), 500), 250));
	if (ImGui::Begin("Information", &isOpen, ImVec2(500.f, 250.f), windowAlpha, flags))
	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}
	ImGui::PopStyleColor();
}


void cMapView::OnResizeEnd(const framework::eDockResize::Enum type, const sRectf &rect) 
{
	if (type == eDockResize::DOCK_WINDOW)
	{
		m_owner->RequestResetDeviceNextFrame();
	}
}


void cMapView::UpdateLookAt()
{
	GetMainCamera().MoveCancel();

	const float centerX = GetMainCamera().m_width / 2;
	const float centerY = GetMainCamera().m_height / 2;
	const Ray ray = GetMainCamera().GetRay((int)centerX, (int)centerY);
	const float distance = m_groundPlane1.Collision(ray.dir);
	if (distance < -0.2f)
	{
		GetMainCamera().m_lookAt = m_groundPlane1.Pick(ray.orig, ray.dir);
	}
	else
	{ // horizontal viewing
		const Vector3 lookAt = GetMainCamera().m_eyePos + GetMainCamera().GetDirection() * 50.f;
		GetMainCamera().m_lookAt = lookAt;
	}

	GetMainCamera().UpdateViewMatrix();
}


// 휠을 움직였을 때,
// 카메라 앞에 박스가 있다면, 박스 정면에서 멈춘다.
void cMapView::OnWheelMove(const float delta, const POINT mousePt)
{
	UpdateLookAt();

	float len = 0;
	const Ray ray = GetMainCamera().GetRay(mousePt.x, mousePt.y);
	Vector3 lookAt = m_groundPlane1.Pick(ray.orig, ray.dir);
	//len = (ray.orig - lookAt).Length();
	//const int lv = m_quadTree.GetLevel(len);
	//const float zoomLen = min(len * 0.1f, (float)(2 << (16-lv)));

	// globe
	len = ray.orig.Length();
	const float zoomLen = max(0, len - 100000) / 10.f;

	GetMainCamera().Zoom(ray.dir, (delta < 0) ? -zoomLen : zoomLen);

	// 지형보다 아래에 카메라가 위치하면, 높이를 조절한다.
	const Vector3 eyePos = m_camera.GetEyePos();
	const float h = m_quadTree.GetHeight(Vector2(eyePos.x, eyePos.z));
	if (eyePos.y < (h + 2))
	{
		const Vector3 newPos(eyePos.x, h + 2, eyePos.z);
		m_camera.SetEyePos(newPos);
	}
}


// Handling Mouse Move Event
void cMapView::OnMouseMove(const POINT mousePt)
{
	const POINT delta = { mousePt.x - m_mousePos.x, mousePt.y - m_mousePos.y };
	m_mousePos = mousePt;

	const bool isCameraMove = true;

	if (m_mouseDown[0] && isCameraMove)
	{
		// Plane
		//Vector3 dir = GetMainCamera().GetDirection();
		//Vector3 right = GetMainCamera().GetRight();
		//dir.y = 0;
		//dir.Normalize();
		//right.y = 0;
		//right.Normalize();
		//GetMainCamera().MoveRight(-delta.x * m_rotateLen * 0.001f);
		//GetMainCamera().MoveFrontHorizontal(delta.y * m_rotateLen * 0.001f);

		// Globe
		Vector3 dir = GetMainCamera().GetDirection();
		Vector3 eyePos = GetMainCamera().GetEyePos();
		Vector3 lookAt = GetMainCamera().GetLookAt();
		Vector3 right;
		Vector3 toCenter = -eyePos.Normal();
		if (dir.DotProduct(toCenter) > 0.9f)
		{
			right = GetMainCamera().GetRight();
		}
		else
		{
			const Vector3 up = GetMainCamera().GetUpVector();
			right = up.CrossProduct(toCenter).Normal();
		}
		
		const Vector3 rotAxisX = dir.CrossProduct(right).Normal();
		Quaternion q1(rotAxisX, delta.x * 0.001f);
		Quaternion q2(right, delta.y * 0.001f);
		eyePos = eyePos * q1 * q2;
		lookAt = lookAt * q1 * q2;
		Vector3 up = (lookAt - eyePos).Normal().CrossProduct(right).Normal();
		GetMainCamera().SetCamera(eyePos, lookAt, up);
	}
	else if (m_mouseDown[1] && isCameraMove)
	{
		m_camera.Yaw2(delta.x * 0.005f, Vector3(0, 1, 0));
		m_camera.Pitch2(delta.y * 0.005f, Vector3(0, 1, 0));
	}
	else if (m_mouseDown[2] && isCameraMove)
	{
		//const float len = GetMainCamera().GetDistance();
		//GetMainCamera().MoveRight(-delta.x * len * 0.001f);
		//GetMainCamera().MoveUp(delta.y * len * 0.001f);
	}

	// 지형보다 아래에 카메라가 위치하면, 높이를 조절한다.
	//const Vector3 eyePos = m_camera.GetEyePos();
	//const float h = m_quadTree.GetHeight(Vector2(eyePos.x, eyePos.z));
	//if (eyePos.y < (h + 2))
	//{
	//	const Vector3 newPos(eyePos.x, h + 2, eyePos.z);
	//	m_camera.SetEyePos(newPos);
	//}
}


// Handling Mouse Button Down Event
void cMapView::OnMouseDown(const sf::Mouse::Button &button, const POINT mousePt)
{
	m_mousePos = mousePt;
	UpdateLookAt();
	SetCapture();

	switch (button)
	{
	case sf::Mouse::Left:
	{
		m_mouseDown[0] = true;
		const Ray ray = GetMainCamera().GetRay(mousePt.x, mousePt.y);
		//Vector3 p1 = m_groundPlane1.Pick(ray.orig, ray.dir);
		auto result = m_quadTree.Pick(ray);
		Vector3 p1 = result.second;

		m_rotateLen = (p1 - ray.orig).Length();// min(500.f, (p1 - ray.orig).Length());

		cAutoCam cam(&m_camera);
		//const gis::Vector2d longLat = m_quadTree.GetLongLat(ray);
		const gis::Vector2d longLat = m_quadTree.GetWGS84(p1);
		gis::LatLonToUTMXY(longLat.y, longLat.x, 52, g_root.m_utmLoc.x, g_root.m_utmLoc.y);
		g_root.m_lonLat = Vector2((float)longLat.x, (float)longLat.y);

		if (eState::PICK_RANGE == m_state)
		{
			m_pickRange.push_back(longLat);
		}
	}
	break;

	case sf::Mouse::Right:
	{
		m_mouseDown[1] = true;

		const Ray ray = GetMainCamera().GetRay(mousePt.x, mousePt.y);
		Vector3 target = m_groundPlane1.Pick(ray.orig, ray.dir);
		const float len = (GetMainCamera().GetEyePos() - target).Length();
	}
	break;

	case sf::Mouse::Middle:
		m_mouseDown[2] = true;
		break;
	}
}


void cMapView::OnMouseUp(const sf::Mouse::Button &button, const POINT mousePt)
{
	const POINT delta = { mousePt.x - m_mousePos.x, mousePt.y - m_mousePos.y };
	m_mousePos = mousePt;
	ReleaseCapture();

	switch (button)
	{
	case sf::Mouse::Left:
		m_mouseDown[0] = false;
		break;
	case sf::Mouse::Right:
		m_mouseDown[1] = false;
		break;
	case sf::Mouse::Middle:
		m_mouseDown[2] = false;
		break;
	}
}


void cMapView::OnEventProc(const sf::Event &evt)
{
	ImGuiIO& io = ImGui::GetIO();
	switch (evt.type)
	{
	case sf::Event::KeyPressed:
		switch (evt.key.code)
		{
		case sf::Keyboard::Return:
			break;

		case sf::Keyboard::Space:
		{
			// 128.467758, 37.171993
			//m_camera.Move();
		}
		break;

		case sf::Keyboard::Left: m_camera.MoveRight(-0.5f); break;
		case sf::Keyboard::Right: m_camera.MoveRight(0.5f); break;
		case sf::Keyboard::Up: m_camera.MoveUp(0.5f); break;
		case sf::Keyboard::Down: m_camera.MoveUp(-0.5f); break;
		}
		break;

	case sf::Event::MouseMoved:
	{
		cAutoCam cam(&m_camera);

		POINT curPos;
		GetCursorPos(&curPos); // sf::event mouse position has noise so we use GetCursorPos() function
		ScreenToClient(m_owner->getSystemHandle(), &curPos);
		POINT pos = { curPos.x - m_viewPos.x, curPos.y - m_viewPos.y };
		OnMouseMove(pos);
	}
	break;

	case sf::Event::MouseButtonPressed:
	case sf::Event::MouseButtonReleased:
	{
		cAutoCam cam(&m_camera);

		POINT curPos;
		GetCursorPos(&curPos); // sf::event mouse position has noise so we use GetCursorPos() function
		ScreenToClient(m_owner->getSystemHandle(), &curPos);
		const POINT pos = { curPos.x - m_viewPos.x, curPos.y - m_viewPos.y };
		if (sf::Event::MouseButtonPressed == evt.type)
		{
			if (m_viewRect.IsIn((float)curPos.x, (float)curPos.y))
				OnMouseDown(evt.mouseButton.button, pos);
		}
		else
		{
			OnMouseUp(evt.mouseButton.button, pos);
		}
	}
	break;

	case sf::Event::MouseWheelScrolled:
	{
		cAutoCam cam(&m_camera);

		POINT curPos;
		GetCursorPos(&curPos); // sf::event mouse position has noise so we use GetCursorPos() function
		ScreenToClient(m_owner->getSystemHandle(), &curPos);
		const POINT pos = { curPos.x - m_viewPos.x, curPos.y - m_viewPos.y };
		OnWheelMove(evt.mouseWheelScroll.delta, pos);
	}
	break;
	}
}


void cMapView::OnResetDevice()
{
	cRenderer &renderer = GetRenderer();

	// update viewport
	sRectf viewRect = { 0, 0, m_rect.Width() - 15, m_rect.Height() - 50 };
	m_camera.SetViewPort(viewRect.Width(), viewRect.Height());

	cViewport vp = GetRenderer().m_viewPort;
	vp.m_vp.Width = viewRect.Width();
	vp.m_vp.Height = viewRect.Height();
	m_renderTarget.Create(renderer, vp, DXGI_FORMAT_R8G8B8A8_UNORM, true, true, DXGI_FORMAT_D24_UNORM_S8_UINT);
}

