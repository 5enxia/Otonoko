#pragma once

#include <string>											// C++ std String lib

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"									// .png Loader

#include <GL/glut.h>										// GLUT

struct vec3d { double x; double y; double z; }; // three dimentions Vector (double)
struct vec2d { double x; double y; double z; }; // two dimenstions Vector (double)

/*
* Texture Class
*/
class MyTexture {
public:
	MyTexture() {}
	MyTexture(std::string fn) {
		pixels = stbi_load(fn.c_str(), &width, &height, &bpp, 0);						// Load Image
		printf("Loaded Texture:%s\n", fn.c_str());											// Print Image Name
	}
	~MyTexture() {stbi_image_free(pixels);}

	void beginTexture();																				// Begin Texture Area
	void endTexture() {glDisable(GL_TEXTURE_2D); glPopMatrix();}				// End Texture Area
	void drawRect(vec3d pos, vec3d rotate, double scale, double s, double t);	// Draw Rect Texture for DEBUG

private:
	int width, height;			// Image Size
	int bpp;							// Bit size per Pixel
	unsigned char *pixels;	// Image Data
};

/*
* Begin Texture Area
*/
void MyTexture::beginTexture() {
	glPushMatrix();
	glEnable(GL_TEXTURE_2D);
	// Load Image Data
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels); 
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// Repeat Setting
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// Size Setting
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}


/*
* Draw Rect Texture for DEBUG
*/
void MyTexture::drawRect(vec3d pos, vec3d rotate, double scale, double s, double t) {
	glTranslated(pos.x, pos.y, pos.z);
	glScaled(scale, scale, scale);
	glRotated(rotate.x, 1, 0, 0); glRotated(rotate.y, 0, 1, 0); glRotated(rotate.z, 0, 0, 1);
	glBegin(GL_QUADS);
	glTexCoord2d(0.0, 0.0); glVertex3d(-width / 2, -height / 2, 0);
	glTexCoord2d(s, 0.0); glVertex3d(width / 2, -height / 2, 0);
	glTexCoord2d(s, t); glVertex3d(width / 2, height / 2, 0);
	glTexCoord2d(0.0, t); glVertex3d(-width / 2, height / 2, 0);
	glEnd();
}
