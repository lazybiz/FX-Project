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

#include <windows.h>

#include <vector>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cfloat>
#include <omp.h>

#include "../image.h"
#include "../window.h"

#define	DEF_ABC_OP( op )																				\
	abc & operator op##= ( const abc & z ) { a op##= z.a; b op##= z.b; c op##= z.c; return *this; }		\
	abc & operator op##= ( const T & val ) { a op##= val; b op##= val; c op##= val; return *this; }		\
	abc operator op ( const abc & z ) const { return abc( a op z.a, b op z.b, c op z.c ); }				\
	abc operator op ( const T & val ) const { return abc( a op val, b op val, c op val ); }

template <typename T> class abc {
	T	a, b, c;

public:
	abc() {}
	abc( const abc & z ) : a(z.a), b(z.b), c(z.c) {}
	abc( T x, T y, T z ) : a(x), b(y), c(z) {}

	void set( T x, T y, T z ) { a = x; b = y; c = z; }

	DEF_ABC_OP( + )
	DEF_ABC_OP( - )
	DEF_ABC_OP( * )
	DEF_ABC_OP( / )

	uint32_t rgb32() const {
		int r = a * 255; if ( r < 0 ) r = 0; else if ( r > 255 ) r = 255;
		int g = b * 255; if ( g < 0 ) g = 0; else if ( g > 255 ) g = 255;
		int b = c * 255; if ( b < 0 ) b = 0; else if ( b > 255 ) b = 255;
		return (r << 16) | (g << 8) | b;
	}

	abc & blend( const abc & z, const T & delta ) {
		a = a * delta + z.a * (1 - delta);
		b = b * delta + z.b * (1 - delta);
		c = c * delta + z.c * (1 - delta);
		return *this;
	}

	// dot product
	inline T operator | ( const abc & z ) const {
		return a * z.a + b * z.b + c * z.c;
	}

	inline T sqr_length() const { return *this | *this; }
	inline T length() const { return sqrt( sqr_length() ); }

	inline void normalize() {
		T sl = sqr_length();
		if ( sl > 0 ) {
			T inv_len = 1. / sqrt( sl );
			a *= inv_len;
			b *= inv_len;
			c *= inv_len;
		}
	}

	// cross product
	abc cross( const abc & z ) const {
		return abc( b * z.c - c * z.b, c * z.a - a * z.c, a * z.b - b * z.a );
	}

	// reflect
	abc operator ^ ( const abc & normal ) const {
		return *this + (normal * -((*this | normal) * 2) );
	}
};

typedef abc <double> vec;
typedef abc <double> rgb;



struct ray {
	vec	m_pos;
	vec	m_dir;
	ray( const vec & p, const vec & d ) : m_pos(p), m_dir(d) {}
	vec operator [] ( double shift ) { return m_pos + (m_dir * shift); }
};



struct obj {
	vec		m_pos;
	rgb		m_rgb;
	double	m_reflection;

	obj( vec pos, rgb color, double reflection ) : m_pos(pos), m_rgb(color), m_reflection(reflection) {}

	virtual bool hit( const ray &, double *, double * ) { return false; }
	virtual vec normal( vec & ) const { return vec( 0, 0, 0 ); }
};

class sphere : public obj {
	double	m_rad;
	double	m_sqr_rad;	// square radius

public:
	sphere( vec pos, double r, rgb color, double reflection ) : obj( pos, color, reflection ), m_rad(r), m_sqr_rad( r * r ) {}

	virtual bool hit( const ray & a_ray, double *da, double *db ) {
		double ocs, ca, hc, hcs;
		vec oc = m_pos - a_ray.m_pos;
		ocs = oc.sqr_length();
		ca = oc | a_ray.m_dir;
		if ( (ocs >= m_sqr_rad) && (ca < DBL_EPSILON) ) return false;
		hcs = m_sqr_rad - ocs + (ca * ca);
		if ( hcs > DBL_EPSILON ) {
			hc = sqrt( hcs );
			*da = ca - hc;
			*db = ca + hc;
			return 1;
		}
		return 0;
	}

	virtual vec normal( vec & isec ) const { return (isec - m_pos) / m_rad; }
};


//
// raytracer constants
//
enum {
	ss_size = 4, // super-sampling factor
	ss_size_sqr = ss_size * ss_size,
	max_reflection_recursion = 5
};

class the_ray_tracer : public window {

//	std::vector <light *>	m_lights;
	std::vector <obj *>		m_objs;

	obj * hit_any( const ray & a_ray, double *closest ) {
		obj * p_obj = 0;
		*closest = DBL_MAX;
		for ( auto & p : m_objs ) {
			double da, db;
			if ( p->hit( a_ray, &da, &db ) ) {
				if ( da < *closest ) {
					*closest = da;
					p_obj = p;
				}
			}
		}
		return p_obj;
	}

	rgb trace( const vec & a_eye, ray & a_ray, unsigned recursion ) {
		rgb	color( 0, 0, 0 );
		if ( recursion ) {
			double	closest;
			obj *p_obj = hit_any( a_ray, &closest );
			if ( p_obj ) {

				vec isec = a_ray[closest];			// get intersection point
				vec norm = p_obj->normal( isec );	// get normal

				vec lights[] = {
					{ -1000,  100, -100 },
					{  1000, -500, -100 } };

				for ( size_t i = 0; i < 2; i++ ) {
					vec light_dir = lights[i] - isec;

					ray	s_ray( isec, light_dir ); // shadow ray
					s_ray.m_dir.normalize();
					s_ray.m_pos = s_ray[DBL_EPSILON + .000000001]; // shoft a little forward

					double tmp;
					if ( !hit_any( s_ray, &tmp ) ) {
						light_dir.normalize();

						double dot = light_dir | norm;
						if ( dot < 0 ) dot = 0;

						vec i2e = isec - a_eye; // intersection to eye direction
						i2e.normalize();
						//vec spec = ; // specular vector
						double s_dot = light_dir | (i2e ^ norm);
						if ( s_dot < 0 ) s_dot = 0;

						color += p_obj->m_rgb * dot + /* specular color */rgb( 1, 1, 1 ) * pow( s_dot, 12 );
					}
				}

				if ( p_obj->m_reflection > 0 ) {
					ray r_ray( isec, a_ray.m_dir ^ norm );
					r_ray.m_dir.normalize();
					r_ray.m_pos = r_ray[DBL_EPSILON + .000000001]; // shoft a little forward
					rgb r_rgb = trace( a_eye, r_ray, recursion - 1 );
					color.blend( r_rgb, 1 - p_obj->m_reflection );
				}
			}
		}
		return color;
	}

public:
	the_ray_tracer( int x, int y, int w, int h ) : window( x, y, w, h, 1 ) {}
	virtual ~the_ray_tracer() {}

	void on_create() {

		// create scene
		m_objs.push_back( new sphere( {  100,  50,   150 },   100, { .8,  1,  1 }, .5 ) );
		m_objs.push_back( new sphere( { -150, -50,   160 },    80, {  0,  0,  0 }, .8 ) );
		m_objs.push_back( new sphere( { -100, 100,   180 },    40, {  1, .7, .7 }, .2 ) );
		m_objs.push_back( new sphere( {    0,   0, 10000 },  9800, { .5, .5, .5 },  0 ) );

		// render scene
		#pragma omp parallel for
		for ( int y = 0; y < m_h; y++ ) {
			for ( int x = 0; x < m_w; x++ ) {
				rgb accum( 0, 0, 0 );
				for ( double sy = y - .5; sy < y + .5; sy += 1. / ss_size )
				for ( double sx = x - .5; sx < x + .5; sx += 1. / ss_size ) {
					vec a_eye(
						 (sx - (m_w - 1) * .5),
						-(sy - (m_h - 1) * .5), 0 );
					ray a_ray( a_eye, vec( 0, 0, 1 ) );
					accum += trace( a_eye, a_ray, max_reflection_recursion );
				}
				accum /= ss_size_sqr;
				m_ptr[m_w * y + x] = accum.rgb32();
			}

			if ( omp_get_thread_num() == 0 ) {
				if ( y % 5 == 0 )
					update();
			}
		}
		update();
	}
};

int APIENTRY WinMain( HINSTANCE hInst, HINSTANCE hPInst, LPSTR lpCmdLine, int nCmdShow )
{
	the_ray_tracer rt( -1, -1, 800, 600 );
	rt.idle( false );
	return 0;
}
