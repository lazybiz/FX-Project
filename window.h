//----------------------------------------------------------------------------
// FX Project
// Copyright (C) 2013 Anton Sazonov (lazybiz)
//
// Permission to copy, use, modify, sell and distribute this software 
// is granted provided this copyright notice appears in all copies. 
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
// Contact: lazybiz@yandex.ru
//----------------------------------------------------------------------------

#ifndef __WINDOW_H__
#define __WINDOW_H__

static const char *	class_name = "x_window_cls_x86";

class window {
	static int	m_ref_count;
	HWND		m_wnd;
	BITMAPINFO	m_bi;

	static LRESULT CALLBACK window_proc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
	{
		window *p = (window *)GetWindowLongPtr( hWnd, GWLP_USERDATA );
		switch ( uMsg ) {
			case WM_CREATE: {
				window *p = (window *)((LPCREATESTRUCT)lParam)->lpCreateParams;
				SetWindowLongPtr( hWnd, GWLP_USERDATA, (LONG_PTR)p );
			} break;

			case WM_PAINT: {
				PAINTSTRUCT	ps;
				BeginPaint( hWnd, &ps );
				if ( p->m_scale == 1 ) {
					SetDIBitsToDevice( ps.hdc, 0, 0, p->m_w, p->m_h, 0, 0, 0, p->m_h, p->m_ptr, &p->m_bi, DIB_RGB_COLORS );
				} else {
					StretchDIBits( ps.hdc, 0, 0, p->m_w * p->m_scale, p->m_h * p->m_scale, 0, 0, p->m_w, p->m_h, p->m_ptr, &p->m_bi, DIB_RGB_COLORS, SRCCOPY );
				}
				EndPaint( hWnd, &ps );
			} return 0;

			case WM_MOUSEMOVE:
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			case WM_MBUTTONDBLCLK: {
				window::ftbl_entry fp_event = p->m_event_table[uMsg - WM_MOUSEMOVE];
				(p->*fp_event)( LOWORD( lParam ) / (float)p->m_scale, HIWORD( lParam ) / (float)p->m_scale, wParam );
			} break;

			case WM_KEYDOWN: if ( (int)wParam != VK_ESCAPE ) break;
			case WM_DESTROY: {
				PostQuitMessage( 0 );
			} break;

			default:
				return DefWindowProc( hWnd, uMsg, wParam, lParam );
		}
		return 0;
	}

public:

	int			m_w, m_h;
	int			m_scale;
	uint32_t *	m_ptr;

	typedef void (window::*ftbl_entry)( float, float, int );
	ftbl_entry	m_event_table[10];

	window( int x, int y, int w, int h, int scale = 1 )
		: m_w(w), m_h(h),
			m_scale(scale),
			m_event_table {
				&window::on_mouse_move,
				&window::on_lbutton_down,
				&window::on_lbutton_up,
				&window::on_lbutton_dblclk,
				&window::on_rbutton_down,
				&window::on_rbutton_up,
				&window::on_rbutton_dblclk,
				&window::on_mbutton_down,
				&window::on_mbutton_up,
				&window::on_mbutton_dblclk } {
		if ( !m_ref_count ) {
			WNDCLASS wc = {};
			wc.lpfnWndProc		= window_proc;
			wc.hInstance		= GetModuleHandle( NULL );
			wc.lpszClassName	= class_name;
			wc.hCursor			= LoadCursor( NULL, IDC_ARROW );
			RegisterClass( &wc );
		}
		m_ref_count++;

		ZeroMemory( &m_bi, sizeof( BITMAPINFO ) );
		m_bi.bmiHeader.biSize			= sizeof( BITMAPINFOHEADER );
		m_bi.bmiHeader.biWidth			= m_w;
		m_bi.bmiHeader.biHeight			= -m_h;
		m_bi.bmiHeader.biPlanes			= 1;
		m_bi.bmiHeader.biBitCount		= 32;
		m_bi.bmiHeader.biCompression	= BI_RGB;

		m_ptr = new uint32_t [m_w * m_h];
		memset( m_ptr, 255, m_w * m_h * 4 );

		if ( x == -1 ) x = GetSystemMetrics( SM_CXSCREEN ) / 2 - m_w * scale / 2;
		if ( y == -1 ) y = GetSystemMetrics( SM_CYSCREEN ) / 2 - m_h * scale / 2;

		m_wnd = CreateWindow( class_name, NULL, WS_POPUP | WS_VISIBLE, x, y, m_w * scale, m_h * scale, NULL, NULL, GetModuleHandle( NULL ), (LPVOID)this );
		update();
	}

	~window() {
		m_ref_count--;
		delete [] m_ptr;
		if ( !m_ref_count ) {
			UnregisterClass( class_name, GetModuleHandle( NULL ) );
		}
	}

	void update() {
		InvalidateRect( m_wnd, NULL, FALSE );
		UpdateWindow( m_wnd );
	}

	void pixel( int x, int y, uint32_t c ) {
		if ( x < 0 || x >= m_w || y < 0 || y >= m_h ) return;
		m_ptr[m_w * y + x] = c;
	}

	void blend_pixel( int x, int y, uint32_t c ) {
		if ( x < 0 || y < 0 || x >= m_w || y >= m_h ) return;

		uint32_t *p = m_ptr + m_w * y + x;
		uint32_t s = *p;

		uint32_t sr, sg, sb;
		sr = (s >> 16) & 0xff;
		sg = (s >>  8) & 0xff;
		sb =  s        & 0xff;

		uint32_t ca, cr, cg, cb;
		ca = (c >> 24) & 0xff;
		cr = (c >> 16) & 0xff;
		cg = (c >>  8) & 0xff;
		cb =  c        & 0xff;

		cr = ((cr * ca) + (sr * (255 - ca))) >> 8;
		cg = ((cg * ca) + (sg * (255 - ca))) >> 8;
		cb = ((cb * ca) + (sb * (255 - ca))) >> 8;

		*p = (ca << 24) | (cr << 16) | (cg << 8) | cb;
	}

	virtual void on_create() {}
	virtual void on_destroy() {}

	virtual bool on_idle() { return true; }

	virtual void on_mouse_move( float, float, int ) {}
	virtual void on_lbutton_down( float, float, int ) {}
	virtual void on_lbutton_up( float, float, int ) {}
	virtual void on_lbutton_dblclk( float, float, int ) {}
	virtual void on_rbutton_down( float, float, int ) {}
	virtual void on_rbutton_up( float, float, int ) {}
	virtual void on_rbutton_dblclk( float, float, int ) {}
	virtual void on_mbutton_down( float, float, int ) {}
	virtual void on_mbutton_up( float, float, int ) {}
	virtual void on_mbutton_dblclk( float, float, int ) {}

	void idle( bool b_loop ) {
		on_create();
		MSG msg;
		if ( b_loop ) {
			for ( ; ; ) {
				if ( PeekMessage( &msg, 0, 0, 0, PM_REMOVE ) ) {
					TranslateMessage( &msg );
					if ( msg.message == WM_QUIT ) break;
					DispatchMessage( &msg );
				} else {
					if ( !on_idle() ) break;
				}
			}
		} else {
			while ( GetMessage( &msg, NULL, 0, 0 ) ) {
				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}
		}
		on_destroy();
	}
};

int window::m_ref_count = 0;

#endif // __WINDOW_H__
