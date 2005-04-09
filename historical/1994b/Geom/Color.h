
/*
 *  Color.h: Simple RGB color model
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   June 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifndef SCI_project_Color_h
#define SCI_project_Color_h 1

class HSVColor;
class Piostream;

class Color {
    double _r, _g, _b;
public:
    Color();
    Color(double, double, double);
    Color(const Color&);
    Color& operator=(const Color&);
    Color(const HSVColor&);
    ~Color();

    Color operator*(const Color&) const;
    Color operator*(double) const;
    Color operator+(const Color&) const;
    Color& operator+=(const Color&);

    void get_color(float color[4]);
    inline double r() const {return _r;}
    inline double g() const {return _g;}
    inline double b() const {return _b;}

    friend void Pio(Piostream&, Color&);
};

class HSVColor {
    double _hue;
    double _sat;
    double _val;
public:
    HSVColor();
    HSVColor(double hue, double sat, double val);
    ~HSVColor();
    HSVColor(const HSVColor&);
    HSVColor& operator=(const HSVColor&);

    inline double hue() const {return _hue;}
    inline double sat() const {return _sat;}
    inline double val() const {return _val;}
};

#endif
