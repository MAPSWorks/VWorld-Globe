
#include "stdafx.h"
#include "observerview.h"
#include "vworldglobe.h"
#include "mapview.h"


using namespace graphic;
using namespace framework;


cObserverView::cObserverView(const string &name) 
	: framework::cDockWindow(name) 
	, m_groundPlane1(Vector3(0, 1, 0), 0)
	, m_groundPlane2(Vector3(0, -1, 0), 0)
	, m_showGround(false)
	, m_tessFactor(1)
	, m_showTerrain(true)
	, m_showWireframe(true)
	, m_showRoute(true)
{
}

cObserverView::~cObserverView() 
{
	m_quadTree.Clear();
}


bool cObserverView::Init(cRenderer &renderer)
{
	const Vector3 eyePos(331843.563f, 478027.719f, 55058.8672f);
	const Vector3 lookAt(331004.031f, 0.000000000f, 157745.281f);
	m_camera.SetCamera(eyePos, lookAt, Vector3(0, 1, 0));
	m_camera.SetProjection(MATH_PI / 4.f, m_rect.Width() / m_rect.Height(), 1, 1000000.f);
	m_camera.SetViewPort(m_rect.Width(), m_rect.Height());
	m_camera.m_name = "Observer Camera";
	m_camera.m_isKeepHorizontal = false;

	sf::Vector2u size((u_int)m_rect.Width() - 15, (u_int)m_rect.Height() - 50);
	cViewport vp = renderer.m_viewPort;
	vp.m_vp.Width = (float)size.x;
	vp.m_vp.Height = (float)size.y;
	m_renderTarget.Create(renderer, vp, DXGI_FORMAT_R8G8B8A8_UNORM, true, true
		, DXGI_FORMAT_D24_UNORM_S8_UINT);

	m_blackLight = GetMainLight();
	m_blackLight.m_ambient = Vector4(0, 0, 0, 1);
	m_blackLight.m_diffuse = Vector4(0, 0, 0, 1);
	m_blackLight.m_specular = Vector4(0, 0, 0, 1);		 

	m_ground.Create(renderer, 100, 100, 10, 10);
	
	if (!m_quadTree.Create(renderer, false))
		return false;

	m_quadTree.m_isShowQuadTree = true;
	m_quadTree.m_isShowDistribute = false;
	m_dbgFrustum.Create(renderer, Matrix44::Identity);
	m_dbgFrustum.m_box.m_color = cColor::GREEN;

	return true;
}


void cObserverView::OnUpdate(const float deltaSeconds)
{
	//m_quadTree.Update(GetRenderer(), deltaSeconds);
}


void cObserverView::OnPreRender(const float deltaSeconds)
{
	cRenderer &renderer = GetRenderer();
	cAutoCam cam(&m_camera);

	renderer.UnbindShaderAll();
	renderer.UnbindTextureAll();

	GetMainCamera().Bind(renderer);
	if (m_quadTree.m_isShowQuadTree)
		GetMainLight().Bind(renderer);
	else
		m_blackLight.Bind(renderer);

	m_quadTree.m_isLight = false;

	if (m_renderTarget.Begin(renderer))
	{
		CommonStates states(renderer.GetDevice());
		if (m_showWireframe)
			renderer.GetDevContext()->RSSetState(states.Wireframe());

		if (m_showGround)
			m_ground.Render(renderer);

		cViewer *viewer = (cViewer*)g_application;
		cCamera3D &mainCam = viewer->m_mapView->m_camera;
		const Ray ray1 = mainCam.GetRay();
		const Ray ray2 = GetMainCamera().GetRay(m_mousePos.x, m_mousePos.y);

		cFrustum frustum;
		frustum.SetFrustum(mainCam.GetViewProjectionMatrix());
		m_quadTree.Render(renderer, deltaSeconds, frustum, ray1, ray2);

		// render autopilot route
		if (m_showRoute && !g_root.m_route.empty())
		{
			for (int i = 0; i < (int)g_root.m_route.size() - 1; ++i)
			{
				auto &lonLat0 = g_root.m_route[i];
				auto &lonLat1 = g_root.m_route[i + 1];

				const Vector3 p0 = gis::GetRelationPos(gis::WGS842Pos(lonLat0));
				const Vector3 p1 = gis::GetRelationPos(gis::WGS842Pos(lonLat1));
				renderer.m_dbgLine.SetLine(p0 + Vector3(0, 20, 0), p1 + Vector3(0, 20, 0), 0.03f);
				renderer.m_dbgLine.Render(renderer);
			}
		}

		m_dbgFrustum.SetFrustum(renderer, frustum);
		m_dbgFrustum.Render(renderer);
	}
	m_renderTarget.End(renderer);
}


void cObserverView::OnRender(const float deltaSeconds)
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
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(min(m_viewRect.Width(), 500), 120));
	if (ImGui::Begin("Information2", &isOpen, ImVec2(500.f, 120.f), windowAlpha, flags))
	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		
		ImGui::Checkbox("Terrain", &m_quadTree.m_isShowTexture);
		ImGui::SameLine();
		//ImGui::Checkbox("Ground", &m_showGround);
		//ImGui::SameLine();
		ImGui::Checkbox("Wireframe", &m_showWireframe);
		ImGui::SameLine();
		ImGui::Checkbox("Quadtree", &m_quadTree.m_isShowQuadTree);
		ImGui::Checkbox("Facility", &m_quadTree.m_isShowFacility);
		ImGui::SameLine();
		ImGui::Checkbox("Level", &m_quadTree.m_isShowLevel);
		ImGui::SameLine();
		ImGui::Checkbox("Loc", &m_quadTree.m_isShowLoc);
		ImGui::SameLine();
		ImGui::Checkbox("Route", &m_showRoute);
		//ImGui::DragInt("Tessellation Factor", &m_tessFactor);
		ImGui::Text("Show Quad Count: %d", m_quadTree.m_showQuadCount);
		ImGui::End();
	}
	ImGui::PopStyleColor();
}


void cObserverView::OnResizeEnd(const framework::eDockResize::Enum type, const sRectf &rect)
{
	if (type == eDockResize::DOCK_WINDOW)
	{
		m_owner->RequestResetDeviceNextFrame();
	}
}


void cObserverView::UpdateLookAt()
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
void cObserverView::OnWheelMove(const float delta, const POINT mousePt)
{
	UpdateLookAt();

	float len = 0;
	const Ray ray = GetMainCamera().GetRay(mousePt.x, mousePt.y);
	Vector3 lookAt = m_groundPlane1.Pick(ray.orig, ray.dir);
	len = (ray.orig - lookAt).Length();

	// zoom in/out
	float zoomLen = 0;
	if (len > 100)
		zoomLen = len * 0.5f;
	else if (len > 50)
		zoomLen = max(1.f, (len / 2.f));
	else
		zoomLen = (len / 3.f);

	//Vector3 eyePos = GetMainCamera().m_eyePos + ray.dir * ((delta <= 0) ? -zoomLen : zoomLen);
	GetMainCamera().Zoom(ray.dir, (delta < 0) ? -zoomLen : zoomLen);
}


// Handling Mouse Move Event
void cObserverView::OnMouseMove(const POINT mousePt)
{
	const POINT delta = { mousePt.x - m_mousePos.x, mousePt.y - m_mousePos.y };
	m_mousePos = mousePt;

	if (m_mouseDown[0])
	{
		Vector3 dir = GetMainCamera().GetDirection();
		Vector3 right = GetMainCamera().GetRight();
		dir.y = 0;
		dir.Normalize();
		right.y = 0;
		right.Normalize();

		GetMainCamera().MoveRight(-delta.x * m_rotateLen * 0.001f);
		GetMainCamera().MoveFrontHorizontal(delta.y * m_rotateLen * 0.001f);
	}
	else if (m_mouseDown[1])
	{
		m_camera.Yaw2(delta.x * 0.005f, Vector3(0, 1, 0));
		m_camera.Pitch2(delta.y * 0.005f, Vector3(0, 1, 0));

		//GetMainCamera().Yaw2(delta.x * 0.005f, Vector3(0, 1, 0));
		//GetMainCamera().Pitch2(delta.y * 0.005f, Vector3(0, 1, 0));
	}
	else if (m_mouseDown[2])
	{
		const float len = GetMainCamera().GetDistance();
		GetMainCamera().MoveRight(-delta.x * len * 0.001f);
		GetMainCamera().MoveUp(delta.y * len * 0.001f);
	}
}


// Handling Mouse Button Down Event
void cObserverView::OnMouseDown(const sf::Mouse::Button &button, const POINT mousePt)
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
		Vector3 p1 = m_groundPlane1.Pick(ray.orig, ray.dir);
		m_rotateLen = (p1 - ray.orig).Length();// min(500.f, (p1 - ray.orig).Length());
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


void cObserverView::OnMouseUp(const sf::Mouse::Button &button, const POINT mousePt)
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


void cObserverView::OnEventProc(const sf::Event &evt)
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


void cObserverView::OnResetDevice()
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

