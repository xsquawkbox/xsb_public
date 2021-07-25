/*
 * ImgWindow.cpp
 *
 * Integration for dear imgui into X-Plane.
 *
 * Copyright (C) 2018,2020, Christopher Collins
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#include "ImgWindow.h"

#include <XPLMDataAccess.h>
#include <XPLMDisplay.h>
#include <XPLMGraphics.h>

static XPLMDataRef		gVrEnabledRef			= nullptr;
static XPLMDataRef		gModelviewMatrixRef		= nullptr;
static XPLMDataRef		gViewportRef			= nullptr;
static XPLMDataRef		gProjectionMatrixRef	= nullptr;
static XPLMDataRef		gFrameRatePeriodRef     = nullptr;

std::shared_ptr<ImgFontAtlas> ImgWindow::sFontAtlas;

ImgWindow::ImgWindow(
	int left,
	int top,
	int right,
	int bottom,
	XPLMWindowDecoration decoration,
	XPLMWindowLayer layer) :
	mIsInVR(false),
	mPreferredLayer(layer),
	mFirstRender(true),
	mFontAtlas(sFontAtlas)
{
    ImFontAtlas *iFontAtlas = nullptr;
    if (mFontAtlas) {
        mFontAtlas->bindTexture();
        iFontAtlas = mFontAtlas->getAtlas();
    }
	mImGuiContext = ImGui::CreateContext(iFontAtlas);
	ImGui::SetCurrentContext(mImGuiContext);
	auto &io = ImGui::GetIO();

	static bool first_init=false;
	if (!first_init) {
		gVrEnabledRef = XPLMFindDataRef("sim/graphics/VR/enabled");
		gModelviewMatrixRef = XPLMFindDataRef("sim/graphics/view/modelview_matrix");
		gViewportRef = XPLMFindDataRef("sim/graphics/view/viewport");
		gProjectionMatrixRef = XPLMFindDataRef("sim/graphics/view/projection_matrix");
        gFrameRatePeriodRef = XPLMFindDataRef("sim/operation/misc/frame_rate_period");
		first_init=true;
	}

	// we render ourselves, we don't use the DrawListsFunc
	io.RenderDrawListsFn = nullptr;
	// set up the Keymap
	io.KeyMap[ImGuiKey_Tab] = XPLM_VK_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = XPLM_VK_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = XPLM_VK_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = XPLM_VK_UP;
	io.KeyMap[ImGuiKey_DownArrow] = XPLM_VK_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = XPLM_VK_PRIOR;
	io.KeyMap[ImGuiKey_PageDown] = XPLM_VK_NEXT;
	io.KeyMap[ImGuiKey_Home] = XPLM_VK_HOME;
	io.KeyMap[ImGuiKey_End] = XPLM_VK_END;
	io.KeyMap[ImGuiKey_Insert] = XPLM_VK_INSERT;
	io.KeyMap[ImGuiKey_Delete] = XPLM_VK_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = XPLM_VK_BACK;
	io.KeyMap[ImGuiKey_Space] = XPLM_VK_SPACE;
    io.KeyMap[ImGuiKey_Enter] = XPLM_VK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = XPLM_VK_ESCAPE;
    io.KeyMap[ImGuiKey_KeyPadEnter] = XPLM_VK_ENTER;
	io.KeyMap[ImGuiKey_A] = XPLM_VK_A;
	io.KeyMap[ImGuiKey_C] = XPLM_VK_C;
	io.KeyMap[ImGuiKey_V] = XPLM_VK_V;
	io.KeyMap[ImGuiKey_X] = XPLM_VK_X;
	io.KeyMap[ImGuiKey_Y] = XPLM_VK_Y;
	io.KeyMap[ImGuiKey_Z] = XPLM_VK_Z;

	// disable window rounding since we're not rendering the frame anyway.
	auto &style = ImGui::GetStyle();
	style.WindowRounding = 0;

	// bind the font
	if (mFontAtlas) {
        mFontTexture = static_cast<GLuint>(reinterpret_cast<intptr_t>(io.Fonts->TexID));
    } else {
        if (iFontAtlas->TexID == nullptr) {
            // fallback binding if an atlas wasn't explicitly set.
            unsigned char *pixels;
            int width, height;
            io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

            // slightly stupid dance around the texture number due to XPLM not using GLint here.
            int texNum = 0;
            XPLMGenerateTextureNumbers(&texNum, 1);
            mFontTexture = (GLuint)texNum;

            // upload texture.
            XPLMBindTexture2d(mFontTexture, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_ALPHA,
                         width,
                         height,
                         0,
                         GL_ALPHA,
                         GL_UNSIGNED_BYTE,
                         pixels);
            io.Fonts->SetTexID((void *)((intptr_t)(mFontTexture)));
        }
    }

	// disable OSX-like keyboard behaviours always - we don't have the keymapping for it.
	io.ConfigMacOSXBehaviors = false;

	// try to inhibit a few resize/move behaviours that won't play nice with our window control.
	io.ConfigWindowsResizeFromEdges = false;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	XPLMCreateWindow_t	windowParams = {
		sizeof(windowParams),
		left,
		top,
		right,
		bottom,
		0,
		DrawWindowCB,
		HandleMouseClickCB,
		HandleKeyFuncCB,
		HandleCursorFuncCB,
		HandleMouseWheelFuncCB,
		reinterpret_cast<void*>(this),
		decoration,
		layer,
		HandleRightClickFuncCB,
	};
	mWindowID = XPLMCreateWindowEx(&windowParams);
}

ImgWindow::~ImgWindow()
{
	ImGui::SetCurrentContext(mImGuiContext);
	if (!mFontAtlas) {
	    // if we didn't have an explicit font atlas, destroy the texture.
        glDeleteTextures(1, &mFontTexture);
    }
	ImGui::DestroyContext(mImGuiContext);
	XPLMDestroyWindow(mWindowID);
}

void
ImgWindow::updateMatrices()
{
	// Get the current modelview matrix, viewport, and projection matrix from X-Plane
	XPLMGetDatavf(gModelviewMatrixRef, mModelView, 0, 16);
	XPLMGetDatavf(gProjectionMatrixRef, mProjection, 0, 16);
	XPLMGetDatavi(gViewportRef, mViewport, 0, 4);
}

static void multMatrixVec4f(GLfloat dst[4], const GLfloat m[16], const GLfloat v[4])
{
	dst[0] = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + v[3] * m[12];
	dst[1] = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + v[3] * m[13];
	dst[2] = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + v[3] * m[14];
	dst[3] = v[0] * m[3] + v[1] * m[7] + v[2] * m[11] + v[3] * m[15];
}

void
ImgWindow::boxelsToNative(int x, int y, int &outX, int &outY)
{
	GLfloat boxelPos[4] = { (GLfloat)x, (GLfloat)y, 0, 1 };
	GLfloat eye[4], ndc[4];

	multMatrixVec4f(eye, mModelView, boxelPos);
	multMatrixVec4f(ndc, mProjection, eye);
	ndc[3] = 1.0f / ndc[3];
	ndc[0] *= ndc[3];
	ndc[1] *= ndc[3];

	outX = static_cast<int>((ndc[0] * 0.5f + 0.5f) * mViewport[2] + mViewport[0]);
	outY = static_cast<int>((ndc[1] * 0.5f + 0.5f) * mViewport[3] + mViewport[1]);
}

/*
 * NB:  This is a modified version of the imGui OpenGL2 renderer - however, because
 *     we need to play nice with the X-Plane GL state management, we cannot use
 *     the upstream one.
 */

void
ImgWindow::RenderImGui(ImDrawData *draw_data)
{
	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	ImGuiIO& io = ImGui::GetIO();
	draw_data->ScaleClipRects(io.DisplayFramebufferScale);

	updateMatrices();

	// We are using the OpenGL fixed pipeline because messing with the
	// shader-state in X-Plane is not very well documented, but using the fixed
	// function pipeline is.

	// 1TU + Alpha settings, no depth, no fog.
	XPLMSetGraphicsState(0, 1, 0, 1, 1, 0, 0);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);
	glDisable(GL_CULL_FACE);
	glEnable(GL_SCISSOR_TEST);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnable(GL_TEXTURE_2D);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glScalef(1.0f, -1.0f, 1.0f);
	glTranslatef(static_cast<GLfloat>(mLeft), static_cast<GLfloat>(-mTop), 0.0f);

	// Render command lists
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
		const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;
		glVertexPointer(2, GL_FLOAT, sizeof(ImDrawVert), (const GLvoid*)((const char*)vtx_buffer + IM_OFFSETOF(ImDrawVert, pos)));
		glTexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert), (const GLvoid*)((const char*)vtx_buffer + IM_OFFSETOF(ImDrawVert, uv)));
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), (const GLvoid*)((const char*)vtx_buffer + IM_OFFSETOF(ImDrawVert, col)));

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)	{
				pcmd->UserCallback(cmd_list, pcmd);
			} else {
			    XPLMBindTexture2d((GLuint)(intptr_t)pcmd->TextureId, 0);

				// Scissors work in viewport space - must translate the coordinates from ImGui -> Boxels, then Boxels -> Native.
				//FIXME: it must be possible to apply the scale+transform manually to the projection matrix so we don't need to doublestep.
				int bTop, bLeft, bRight, bBottom;
				translateImguiToBoxel(pcmd->ClipRect.x, pcmd->ClipRect.y, bLeft, bTop);
				translateImguiToBoxel(pcmd->ClipRect.z, pcmd->ClipRect.w, bRight, bBottom);
				int nTop, nLeft, nRight, nBottom;
				boxelsToNative(bLeft, bTop, nLeft, nTop);
				boxelsToNative(bRight, bBottom, nRight, nBottom);
				glScissor(nLeft, nBottom, nRight-nLeft, nTop-nBottom);
				glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer);
			}
			idx_buffer += pcmd->ElemCount;
		}
	}

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	// Restore modified state
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glPopAttrib();
	glPopClientAttrib();
}

void
ImgWindow::translateToImguiSpace(int inX, int inY, float &outX, float &outY)
{
	outX = static_cast<float>(inX - mLeft);
	if (outX < 0.0f || outX > (float)(mRight - mLeft)) {
		outX = -FLT_MAX;
		outY = -FLT_MAX;
		return;
	}
	outY = static_cast<float>(mTop-inY);
	if (outY < 0.0f || outY > (float)(mTop - mBottom)) {
		outX = -FLT_MAX;
		outY = -FLT_MAX;
		return;
	}
}

void
ImgWindow::translateImguiToBoxel(float inX, float inY, int &outX, int &outY)
{
	outX = (int)(mLeft + inX);
	outY = (int)(mTop - inY);
}


void
ImgWindow::updateImgui()
{
	ImGui::SetCurrentContext(mImGuiContext);
	auto &io = ImGui::GetIO();

	// transfer the window geometry to ImGui
	XPLMGetWindowGeometry(mWindowID, &mLeft, &mTop, &mRight, &mBottom);

	float win_width = static_cast<float>(mRight - mLeft);
	float win_height = static_cast<float>(mTop - mBottom);

	io.DeltaTime = XPLMGetDataf(gFrameRatePeriodRef);
	io.DisplaySize = ImVec2(win_width, win_height);
	// in boxels, we're always scale 1, 1.
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2((float) 0.0, (float) 0.0), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_Always);

	// and construct the window
	ImGui::Begin(mWindowTitle.c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
	buildInterface();
	ImGui::End();

	// finally, handle window focus.
	int hasKeyboardFocus = XPLMHasKeyboardFocus(mWindowID);
	if (io.WantTextInput && !hasKeyboardFocus) {
		XPLMTakeKeyboardFocus(mWindowID);
	}
	else if (!io.WantTextInput && hasKeyboardFocus) {
		XPLMTakeKeyboardFocus(nullptr);
		// reset keysdown otherwise we'll think any keys used to defocus the keyboard are still down!
		for (auto &key : io.KeysDown) {
			key = false;
		}
	}
	mFirstRender = false;
}

void
ImgWindow::DrawWindowCB(XPLMWindowID /* inWindowID */, void *inRefcon)
{
	auto *thisWindow = reinterpret_cast<ImgWindow *>(inRefcon);

	thisWindow->updateImgui();

	ImGui::SetCurrentContext(thisWindow->mImGuiContext);
	ImGui::Render();

	thisWindow->RenderImGui(ImGui::GetDrawData());
}

int
ImgWindow::HandleMouseClickCB(XPLMWindowID /* inWindowID */, int x, int y, XPLMMouseStatus inMouse, void *inRefcon)
{
	auto *thisWindow = reinterpret_cast<ImgWindow *>(inRefcon);
	return thisWindow->HandleMouseClickGeneric(x, y, inMouse, 0);
}

int
ImgWindow::HandleMouseClickGeneric(int x, int y, XPLMMouseStatus inMouse, int button)
{
	ImGui::SetCurrentContext(mImGuiContext);
	ImGuiIO& io = ImGui::GetIO();

	float outX, outY;
	translateToImguiSpace(x, y, outX, outY);

	io.MousePos = ImVec2(outX, outY);

	switch (inMouse) {
	case xplm_MouseDown:
	case xplm_MouseDrag:
		io.MouseDown[button] = true;
		break;
	case xplm_MouseUp:
		io.MouseDown[button] = false;
		break;
	default:
		// dunno!
		break;
	}
	return 1;
}


void
ImgWindow::HandleKeyFuncCB(
	XPLMWindowID         inWindowID,
	char                 inKey,
	XPLMKeyFlags         inFlags,
	char                 inVirtualKey,
	void *               inRefcon,
	int                  losingFocus)
{
	auto *thisWindow = reinterpret_cast<ImgWindow *>(inRefcon);
	ImGui::SetCurrentContext(thisWindow->mImGuiContext);
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard) {
		auto vk = static_cast<unsigned char>(inVirtualKey);
		io.KeysDown[vk] = (inFlags & xplm_DownFlag) == xplm_DownFlag;
		io.KeyShift = (inFlags & xplm_ShiftFlag) == xplm_ShiftFlag;
		io.KeyAlt = (inFlags & xplm_OptionAltFlag) == xplm_OptionAltFlag;
		io.KeyCtrl = (inFlags & xplm_ControlFlag) == xplm_ControlFlag;

		if ((inFlags & xplm_DownFlag) == xplm_DownFlag
			&& !io.KeyCtrl
			&& !io.KeyAlt
			&& isprint(inKey)) {
			char smallStr[2] = { inKey, 0 };
			io.AddInputCharactersUTF8(smallStr);
		}
	}
}

XPLMCursorStatus
ImgWindow::HandleCursorFuncCB(
	XPLMWindowID         inWindowID,
	int                  x,
	int                  y,
	void *               inRefcon)
{
	auto *thisWindow = reinterpret_cast<ImgWindow *>(inRefcon);
	ImGui::SetCurrentContext(thisWindow->mImGuiContext);
	ImGuiIO& io = ImGui::GetIO();
	float outX, outY;
	thisWindow->translateToImguiSpace(x, y, outX, outY);
	io.MousePos = ImVec2(outX, outY);
	
	// Exclude resize regions handled by XPLM for self-styled windows:
	if (thisWindow->IsInsideSim() && thisWindow->bHandleWndResize &&
		(x < (thisWindow->mLeft + WND_RESIZE_LEFT_WIDTH) ||
		 x > (thisWindow->mRight - WND_RESIZE_RIGHT_WIDTH) ||
		 y > (thisWindow->mTop - WND_RESIZE_TOP_WIDTH) ||
		 y < (thisWindow->mBottom + WND_RESIZE_BOTTOM_WIDTH)))
	{
		// Defer to XPLM's hand cursor for managed resize grab regions:
		io.MouseDrawCursor = false;
		return xplm_CursorDefault;
	}
	
	// Have ImGui take over the mouse cursor for the rest:
	io.MouseDrawCursor = true;
	return xplm_CursorHidden;
}

int
ImgWindow::HandleMouseWheelFuncCB(
	XPLMWindowID         inWindowID,
	int                  x,
	int                  y,
	int                  wheel,
	int                  clicks,
	void *               inRefcon)
{
	auto *thisWindow = reinterpret_cast<ImgWindow *>(inRefcon);
	ImGui::SetCurrentContext(thisWindow->mImGuiContext);
	ImGuiIO& io = ImGui::GetIO();

	float outX, outY;
	thisWindow->translateToImguiSpace(x, y, outX, outY);
	io.MousePos = ImVec2(outX, outY);
	switch (wheel) {
	case 0:
		io.MouseWheel = static_cast<float>(clicks);
		break;
	case 1:
		io.MouseWheelH = static_cast<float>(clicks);
		break;
	default:
		// unknown wheel
		break;
	}
	return 1;
}

int
ImgWindow::HandleRightClickFuncCB(XPLMWindowID /* inWindowID */, int x, int y, XPLMMouseStatus inMouse, void *inRefcon)
{
	auto *thisWindow = reinterpret_cast<ImgWindow *>(inRefcon);
	return thisWindow->HandleMouseClickGeneric(x, y, inMouse, 1);
}


void
ImgWindow::SetWindowTitle(const std::string &title)
{
	mWindowTitle = title;
	XPLMSetWindowTitle(mWindowID, mWindowTitle.c_str());
}

void
ImgWindow::SetVisible(bool inIsVisible)
{
	if (inIsVisible)
		moveForVR();
	if (GetVisible() == inIsVisible) {
		// if the state is already correct, no-op.
		return;
	}
	if (inIsVisible) {
		if (!onShow()) {
			// chance to early abort.
			return;
		}
	}
	XPLMSetWindowIsVisible(mWindowID, inIsVisible);
}

void
ImgWindow::moveForVR()
{
	// if we're trying to display the window, check the state of the VR flag
	// - if we're VR enabled, explicitly move the window to the VR world.
	if (XPLMGetDatai(gVrEnabledRef)) {
			XPLMSetWindowPositioningMode(mWindowID, xplm_WindowVR, 0);
			mIsInVR = true;
		} else {
			if (mIsInVR) {
				XPLMSetWindowPositioningMode(mWindowID, mPreferredLayer, -1);
				mIsInVR = false;
			}
		}
}

bool
ImgWindow::GetVisible() const
{
	return XPLMGetWindowIsVisible(mWindowID) != 0;
}


bool
ImgWindow::onShow()
{
	return true;
}

void
ImgWindow::SafeDelete()
{
	sPendingDestruction.push(this);
	if (sSelfDestructHandler == nullptr) {
        XPLMCreateFlightLoop_t flParams{
            sizeof(flParams),
            xplm_FlightLoop_Phase_BeforeFlightModel,
            &ImgWindow::SelfDestructCallback,
            nullptr,
        };
        sSelfDestructHandler = XPLMCreateFlightLoop(&flParams);
	}
	XPLMScheduleFlightLoop(sSelfDestructHandler, -1, 1);
}

std::queue<ImgWindow *>  ImgWindow::sPendingDestruction;
XPLMFlightLoopID         ImgWindow::sSelfDestructHandler = nullptr;

float
ImgWindow::SelfDestructCallback(float inElapsedSinceLastCall,
                                float inElapsedTimeSinceLastFlightLoop,
                                int inCounter,
                                void *inRefcon)
{
    while (!sPendingDestruction.empty()) {
        auto *thisObj = sPendingDestruction.front();
        sPendingDestruction.pop();
        delete thisObj;
    }
    return 0;
}

