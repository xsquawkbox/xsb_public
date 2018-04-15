# XSB Public Sources

The sources in this repository are shared with the greater X-Plane developer
community in the hope that it may save somebody a headache some day.

These are sources shared from the XSquawkBox codebase - but have been
exclusively written by myself, allowing me to use a public license.

## Licesing Note

These sources are released under the LGPL - I've tried to ensure that they can
be practically used through subclassing (which is how they're used in XSB)

Whilst you can directly link them into your code without other obligations, if
you improve or alter the code contained within directly, I ask that you make 
those changes available as per the licese.

## Prerequisites

Components in this library assume the availability of the X-Plane SDK and may
rely upon unstable/still-in-development features.

Some components in this library may assume availability of the 
[libxplanemp](https://github.com/kuroneko/libxplanemp) sources and functions.

## Components in this Repository

* `ImgWindow` - Wrapper for the [dear imgui](https://github.com/ocornut/imgui)
  Immediate Mode GUI library

