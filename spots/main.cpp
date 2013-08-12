#include <windows.h>

#include <iostream>
#include <sstream>
#include <cstdint>
#include <cstdio>
#include <cmath>

#include <chrono>
#include <random>
#include <vector>

#include "../window.h"
#include "../stack_blur8.h"

#define MAKE_COLOR( r, g, b, a )	(((a) << 24) | ((r) << 16) | ((g) << 8) | (b))

static std::mt19937 rng;
static std::uniform_int_distribution <int> fdist( 0, 1000000 );

float my_rand( float min, float max ) {
	float r = fdist( rng ) / 1000000.;
	return r * (max - min) + min;
}

uint32_t lerp_color( uint32_t *a, uint32_t *b, uint32_t delta /* 0 - 255 */ ) {
	uint32_t aa, ar, ag, ab;
	aa = (*a >> 24) & 0xff;
	ar = (*a >> 16) & 0xff;
	ag = (*a >>  8) & 0xff;
	ab =  *a        & 0xff;

	uint32_t ba, br, bg, bb;
	ba = (*b >> 24) & 0xff;
	br = (*b >> 16) & 0xff;
	bg = (*b >>  8) & 0xff;
	bb =  *b        & 0xff;

	aa = ((aa * delta) + (ba * (255 - delta))) >> 8;
	ar = ((ar * delta) + (br * (255 - delta))) >> 8;
	ag = ((ag * delta) + (bg * (255 - delta))) >> 8;
	ab = ((ab * delta) + (bb * (255 - delta))) >> 8;

	return (aa << 24) | (ar << 16) | (ag << 8) | ab;
}

struct rect {
	int	x0, y0;
	int	x1, y1;
};

class live_spot {
	int			m_maxx;
	int			m_maxy;
	float		m_maxr;

	float		m_x, m_y;
	float		m_r, m_r0, m_r1;
	float		m_dx, m_dy;		// motion delta
	float		m_ddx, m_ddy;	// dx delta
	uint32_t	m_a, m_maxa;
	int			m_lifephase;	// phase [0 <= m_lifetime]
	int			m_lifetime;		// cycles

	stack_blur8	m_blur;
	int			m_blur_radius;
	//int			m_blur_radius_max;

	image <uint8_t> m_temp_buffer;
	rect		m_rc;

	void blend_pixel( uint32_t *ptr, int pitch, int x, int y, uint32_t c ) {
		uint32_t *p = ptr + pitch * y + x;
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

		*p = /*(ca << 24) |*/ (cr << 16) | (cg << 8) | cb;
	}

	/* x and y can be out of bound */
	bool render_spot( image <uint8_t> buffer, float x, float y, float r ) {
		m_rc.x0 = floor( x - r );
		m_rc.y0 = floor( y - r );
		m_rc.x1 = ceil( x + r );
		m_rc.y1 = ceil( y + r );

		// ugly? yes!
		if ( m_rc.x0 < 0 ) m_rc.x0 = 0; else if ( m_rc.x0 > buffer.width()  ) m_rc.x0 = buffer.width();
		if ( m_rc.y0 < 0 ) m_rc.y0 = 0; else if ( m_rc.y0 > buffer.height() ) m_rc.y0 = buffer.height();
		if ( m_rc.x1 >= buffer.width()  ) m_rc.x1 = buffer.width();  else if ( m_rc.x1 < 0 ) m_rc.x1 = 0;
		if ( m_rc.y1 >= buffer.height() ) m_rc.y1 = buffer.height(); else if ( m_rc.y1 < 0 ) m_rc.y1 = 0;
		if ( m_rc.x0 == m_rc.x1 || m_rc.y0 == m_rc.y1 ) return false;

		for ( int iy = m_rc.y0; iy < m_rc.y1; iy++ ) {
			for ( int ix = m_rc.x0; ix < m_rc.x1; ix++ ) {
				float dx = x - ix;
				float dy = y - iy;
				float d = sqrt( dx * dx + dy * dy );
				uint8_t alpha;
				if ( d < r ) {
					float rd = r - d;
					if ( rd < 1 ) {
						alpha = (uint8_t)(rd * 255);
					} else alpha = 255;
				} else alpha = 0;
				*(buffer.pix_ptr( ix, iy )) = alpha;
			}
		}
		return true;
	}

public:
	live_spot( int w, int h, int maxr, int blur_radius/*, int blur_radius_max*/ )
		: m_maxx(w), m_maxy(h), m_maxr(maxr), m_blur_radius(blur_radius), m_temp_buffer( new uint8_t [w * h], w, h, w ) {
		m_lifephase = m_lifetime = 0;
	}

	~live_spot() {
		delete [] m_temp_buffer.row_ptr( 0 );
	}

	void render( int w, int h ) {

		if ( !render_spot( m_temp_buffer, m_x, m_y, m_r ) ) return; // out of screen

		// expand bounds
		m_rc.x0 -= m_blur_radius;
		m_rc.y0 -= m_blur_radius;
		m_rc.x1 += m_blur_radius;
		m_rc.y1 += m_blur_radius;
		if ( m_rc.x0 < 0 ) m_rc.x0 = 0;
		if ( m_rc.y0 < 0 ) m_rc.y0 = 0;
		if ( m_rc.x1 >= w ) m_rc.x1 = w;
		if ( m_rc.y1 >= h ) m_rc.y1 = h;

		image <uint8_t> subimg( m_temp_buffer.pix_ptr( m_rc.x0, m_rc.y0 ), m_rc.x1 - m_rc.x0, m_rc.y1 - m_rc.y0, m_temp_buffer.stride() );
		m_blur.process( subimg, m_blur_radius, m_blur_radius );
	}

	void blit( uint32_t *dst, int w, int h ) {
		for ( int y = m_rc.y0; y < m_rc.y1; y++ ) {
			for ( int x = m_rc.x0; x < m_rc.x1; x++ ) {
				uint32_t c = 0xffffff;
				c |= (((uint32_t)(*(m_temp_buffer.pix_ptr( x, y ))) * m_a) >> 8) << 24;
				blend_pixel( dst, w, x, y, c );

				// clear old value
				*(m_temp_buffer.pix_ptr( x, y )) = 0;
			}
		}
	}

	void lifecycle() {
		if ( m_lifephase == m_lifetime ) {
			std::uniform_int_distribution <int> fdist_10_255( 10, 255 );
			m_x = my_rand( 0, m_maxx - 1 );
			m_y = my_rand( 0, m_maxy - 1 );
			m_dx = my_rand( 0, 1 ) - .5;
			m_dy = my_rand( 0, 1 ) - .5;
			m_ddx = my_rand( 0, .05 ) - .05;
			m_ddy = my_rand( 0, .05 ) - .05;
			m_r1 = my_rand( m_maxr / 5., m_maxr );
			m_r0 = my_rand( m_r1 / 2, m_r1 );

			// we must calc. depth value for depth-of-field fx.
			//m_maxa = (float)m_blur_radius / m_blur_radius_max * 255;//fdist_10_255( rng );
			m_maxa = fdist_10_255( rng );

			m_lifetime = (int)my_rand( 150, 800 );
			m_lifephase = 0;
		}

		m_x += sin( m_dx ) * .5;
		m_y += cos( m_dy ) * .5;
		m_dx += m_ddx;
		m_dy += m_ddy;

		m_r = m_r0 + (m_r1 - m_r0) / (m_lifetime * m_lifephase + 1); // interpolate radius

		if ( m_lifephase <= 50 ) {
			m_a = (uint8_t)(m_lifephase / 50. * m_maxa);
		} else
		if ( m_lifephase > m_lifetime - 50 ) {
			m_a = (uint8_t)(m_maxa - ((m_lifephase - (m_lifetime - 50)) / 50. * m_maxa));
		}

		m_lifephase++;
	}
};

class the_app : public window {
	uint32_t *					m_background;
	std::vector <live_spot *>	m_spots;
	std::vector <int>			m_blurriness;

public:
	the_app( int x, int y, int w, int h, int scale = 1 ) : window( x, y, w, h, scale ) {}
	virtual ~the_app() {}

	void on_create() {
		m_background = new uint32_t [m_w * m_h];

		// generate background
		float half_diag = sqrt( (m_w / 2) * (m_w / 2) + (m_h / 2) * (m_h / 2) );
		for ( int y = 0; y < m_h; y++ ) {
			for ( int x = 0; x < m_w; x++ ) {
				float dx = x - m_w / 2;
				float dy = y - m_h / 2;
				float d = sqrt( dx * dx + dy * dy );

				uint32_t a = 0x0000ff;
				uint32_t b = MAKE_COLOR( 100, 190, 250, 0 );
				m_background[m_w * y + x] = lerp_color( &a, &b, d / half_diag * 255 );
			}
		}

		for ( size_t i = 0; i < 64; i++ ) {
			std::uniform_int_distribution <int> fdist( 1, 10 );
			m_spots.push_back( new live_spot( m_w, m_h, 50, fdist( rng )/*, 10*/ ) );
		}
	}

	void on_destroy() {
		for ( auto & i : m_spots ) {
			delete i;
		}
		delete [] m_background;
	}

	bool on_idle() {
		memcpy( m_ptr, m_background, m_w * m_h * 4 );

		//#pragma omp parallel for
		for ( size_t i = 0; i < m_spots.size(); i++ ) { // OpenMP work only with ints, i.e. with indices
			m_spots[i]->lifecycle();
			m_spots[i]->render( m_w, m_h );
		}

		// blit in one thread
		for ( auto & i : m_spots ) {
			i->blit( m_ptr, m_w, m_h );
		}

		update();
		Sleep( 20 );
		return true; // continue
	}
};

static the_app * app;

int APIENTRY WinMain( HINSTANCE hInst, HINSTANCE hPInst, LPSTR lpCmdLine, int nCmdShow )
{
	rng.seed( std::chrono::system_clock::to_time_t( std::chrono::high_resolution_clock::now() ) );
	app = new the_app( -1, -1, 1280, 720 );
	app->idle( true /* call on_idle() ? */ );
	delete app;
	return 0;
}
