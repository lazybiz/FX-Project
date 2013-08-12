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


//
// Stack Blur algorithm implementation. Thanks to Mario Klingemann - the author.
//  and Maxim Shemanarev for C++ implementation.
//

#ifndef	__STACK_BLUR8_H__
#define	__STACK_BLUR8_H__

static uint16_t const g_stack_blur8_mul[255] = {
	512,512,456,512,328,456,335,512,405,328,271,456,388,335,292,512,
	454,405,364,328,298,271,496,456,420,388,360,335,312,292,273,512,
	482,454,428,405,383,364,345,328,312,298,284,271,259,496,475,456,
	437,420,404,388,374,360,347,335,323,312,302,292,282,273,265,512,
	497,482,468,454,441,428,417,405,394,383,373,364,354,345,337,328,
	320,312,305,298,291,284,278,271,265,259,507,496,485,475,465,456,
	446,437,428,420,412,404,396,388,381,374,367,360,354,347,341,335,
	329,323,318,312,307,302,297,292,287,282,278,273,269,265,261,512,
	505,497,489,482,475,468,461,454,447,441,435,428,422,417,411,405,
	399,394,389,383,378,373,368,364,359,354,350,345,341,337,332,328,
	324,320,316,312,309,305,301,298,294,291,287,284,281,278,274,271,
	268,265,262,259,257,507,501,496,491,485,480,475,470,465,460,456,
	451,446,442,437,433,428,424,420,416,412,408,404,400,396,392,388,
	385,381,377,374,370,367,363,360,357,354,350,347,344,341,338,335,
	332,329,326,323,320,318,315,312,310,307,304,302,299,297,294,292,
	289,287,285,282,280,278,275,273,271,269,267,265,263,261,259 };

static uint8_t const g_stack_blur8_shr[255] = {
	 9, 11, 12, 13, 13, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 
	17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 19, 
	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20,
	20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 21,
	21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
	21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 
	22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
	22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 
	23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
	23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
	23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 
	23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 
	24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
	24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
	24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
	24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24 };

template <class T> class image {
	T *		m_ptr;
	int		m_width, m_height, m_stride;

public:
	image( T * p, int w, int h, int s ) : m_ptr(p), m_width(w), m_height(h), m_stride(s) {}

	int width() const { return m_width; }
	int height() const { return m_height; }
	int stride() const { return m_stride; }
	T * row_ptr( int y ) const { return m_ptr + y * m_stride; }
	T * pix_ptr( int x, int y ) const { return row_ptr( y ) + x; }
};

class stack_blur8 {
public:
	void process( image <uint8_t> & img, unsigned rx, unsigned ry ) {
		unsigned x, y, xp, yp, i;
		unsigned stack_ptr;
		unsigned stack_start;
		const uint8_t * src_pix_ptr;
		uint8_t * dst_pix_ptr;
		unsigned pix;
		unsigned stack_pix;
		unsigned sum;
		unsigned sum_in;
		unsigned sum_out;
		unsigned w   = img.width();
		unsigned h   = img.height();
		unsigned wm  = w - 1;
		unsigned hm  = h - 1;
		unsigned div;
		unsigned mul_sum;
		unsigned shr_sum;

		uint8_t * stack;

		if ( rx > 0 ) {
			if ( rx > 254 ) rx = 254;

			div = rx * 2 + 1;
			mul_sum = g_stack_blur8_mul[rx];
			shr_sum = g_stack_blur8_shr[rx];

			stack = new uint8_t [div];

			for ( y = 0; y < h; y++ ) {
				sum = sum_in = sum_out = 0;
				src_pix_ptr = img.pix_ptr(0, y);
				pix = *src_pix_ptr;
				for ( i = 0; i <= rx; i++ ) {
					stack[i] = pix;
					sum     += pix * (i + 1);
					sum_out += pix;
				}
				for ( i = 1; i <= rx; i++ ) {
					if ( i <= wm ) src_pix_ptr += 1;//Img::pix_step; 
					pix = *src_pix_ptr; 
					stack[i + rx] = pix;
					sum    += pix * (rx + 1 - i);
					sum_in += pix;
				}
				stack_ptr = rx;
				xp = rx;
				if ( xp > wm ) xp = wm;
				src_pix_ptr = img.pix_ptr( xp, y );
				dst_pix_ptr = img.pix_ptr(  0, y );
				for ( x = 0; x < w; x++ ) {
					*dst_pix_ptr = (sum * mul_sum) >> shr_sum;
					dst_pix_ptr += 1;//Img::pix_step;
					sum -= sum_out;
					stack_start = stack_ptr + div - rx;
					if ( stack_start >= div ) stack_start -= div;
					sum_out -= stack[stack_start];
					if ( xp < wm ) {
						src_pix_ptr += 1;//Img::pix_step;
						pix = *src_pix_ptr;
						++xp;
					}
					stack[stack_start] = pix;
					sum_in += pix;
					sum    += sum_in;
					++stack_ptr;
					if ( stack_ptr >= div ) stack_ptr = 0;
					stack_pix = stack[stack_ptr];
					sum_out += stack_pix;
					sum_in  -= stack_pix;
				}
			}
			delete [] stack;
		}

		if ( ry > 0 ) {
			if ( ry > 254 ) ry = 254;
			div = ry * 2 + 1;
			mul_sum = g_stack_blur8_mul[ry];
			shr_sum = g_stack_blur8_shr[ry];

			stack = new uint8_t [div];

			int stride = img.stride();
			for ( x = 0; x < w; x++ ) {
				sum = sum_in = sum_out = 0;
				src_pix_ptr = img.pix_ptr( x, 0 );
				pix = *src_pix_ptr;
				for ( i = 0; i <= ry; i++ ) {
					stack[i] = pix;
					sum     += pix * (i + 1);
					sum_out += pix;
				}
				for ( i = 1; i <= ry; i++ ) {
					if ( i <= hm ) src_pix_ptr += stride; 
					pix = *src_pix_ptr; 
					stack[i + ry] = pix;
					sum    += pix * (ry + 1 - i);
					sum_in += pix;
				}
				stack_ptr = ry;
				yp = ry;
				if ( yp > hm ) yp = hm;
				src_pix_ptr = img.pix_ptr( x, yp );
				dst_pix_ptr = img.pix_ptr( x,  0 );
				for ( y = 0; y < h; y++ ) {
					*dst_pix_ptr = (sum * mul_sum) >> shr_sum;
					dst_pix_ptr += stride;
					sum -= sum_out;
					stack_start = stack_ptr + div - ry;
					if ( stack_start >= div ) stack_start -= div;
					sum_out -= stack[stack_start];
					if ( yp < hm ) {
						src_pix_ptr += stride;
						pix = *src_pix_ptr;
						++yp;
					}
					stack[stack_start] = pix;
					sum_in += pix;
					sum    += sum_in;
					++stack_ptr;
					if ( stack_ptr >= div ) stack_ptr = 0;
					stack_pix = stack[stack_ptr];
					sum_out += stack_pix;
					sum_in  -= stack_pix;
				}
			}
		}
		delete [] stack;
	}
};

#endif // __STACK_BLUR_H__
