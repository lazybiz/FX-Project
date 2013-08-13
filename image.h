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

#ifndef __IMAGE_H__
#define __IMAGE_H__

template <class T> class image {
	T *		m_ptr;
	int		m_width, m_height, m_stride;

public:
	image( T * p, int w, int h, int s )
		: m_ptr(p), m_width(w), m_height(h), m_stride(s) {}

	int width() const { return m_width; }
	int height() const { return m_height; }
	int stride() const { return m_stride; }
	T * ptr() const { return m_ptr; }
	T * row_ptr( int y ) const { return m_ptr + y * m_stride; }
	T * pix_ptr( int x, int y ) const { return row_ptr( y ) + x; }
};

#endif // __IMAGE_H__
