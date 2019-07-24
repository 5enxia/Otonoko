#pragma once
#include "stdafx.h"

#include "MyTexture.h" // Texture
#include  "OBJ_Loader.h" // OBJ LOADER

namespace Viewer {

	// CAMERA
	vec3d camPos, camLookAt;
	unsigned char mouseFlag = GL_FALSE;
	vec2d angle;

	// MODEL
	objl::Loader miku;

	// texture
	MyTexture mikuTextrues[4] = {
		MyTexture("Miku/Miku.png"),
		MyTexture("Miku/Miku2.png"),
		MyTexture("Miku/Miku3.png"),
		MyTexture("Miku/Miku1.png"),
	};

	double degX,degY,degZ= 0;
	double bias = 45; // Bias (degrees)

	void drawAxis(float len);
	void drawModel(objl::Loader &model, MyTexture *textures, vec3d pos, vec3d rotate, float scale);

	// --------------------------setup---------------------------------------	
	void setup(double width, double height){
		GLdouble aspect = (GLdouble)width/(GLdouble) height;
		glClearColor(0, 0, 0, 1);
		glViewport(0, 0, width, height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(60.0, aspect, 0.01, 20.0);

		gluLookAt(0.0, 1.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0); 

		glShadeModel(GL_SMOOTH);

		// Load Model 
		miku.LoadFile("Miku/Miku.obj");
		for (int i = 0; i < miku.LoadedMeshes.size(); i++) {
			printf("%d\n", miku.LoadedMeshes[i].Vertices.size());
		}
	}

	// -----------------------update--------------------------
	void update(double ex, double ey, double ez){
		degX =  (180/PI) * asin(ex);
		degY = (180 / PI) * asin(ey);
		degZ = (180 / PI) * asin(ez);
	}

	// ------------------------draw-------------------------
	void draw(){
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // OpenGLによる描画
		glEnable(GL_DEPTH_TEST);

		// axis
		glPushMatrix();
		glTranslatef(-1.2,-1.2,0);
		drawAxis(0.25);
		double div = 0.3;
		glRasterPos2f(0 + div ,0);glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24,'x');
		glRasterPos2f(0,0 + div);glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24,'y');
		glRasterPos2f(0 - div,0 - div);	glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24,'z');
		glPopMatrix();

		// Cube
		glPushMatrix();
		glRotatef(degX - 90,1,0,0); // x
		glRotatef(degY,0,1,0); // y
		glRotatef(degZ,0,0,1); // z
		drawAxis(0.5);
		glutWireCube(1);
		glPopMatrix();

		// Model
		drawModel(miku, mikuTextrues, {0,0,-1}, {-90,0,0}, 1);
		glDisable(GL_DEPTH_TEST);
		glFlush();
	}

	// -----------------------------drawAxis------------------------------------
	void drawAxis(float len){
		glLineWidth(3);
		// x
		glColor3f(1, 0, 0);
		glBegin(GL_LINES);glVertex3f(0,0,0);glVertex3f(len,0,0);glEnd();
		// y
		glColor3f(0,1,0);
		glBegin(GL_LINES);glVertex3f(0,0,0);glVertex3f(0,len,0);	glEnd();
		// z
		glColor3f(0,0,1);
		glBegin(GL_LINES);glVertex3f(0,0,0);glVertex3f(0,0,len);glEnd();
		
		glColor3f(1.0, 1.0, 1.0);
	}

	
	// -----------------------------drawModel------------------------------------
	void drawModel(objl::Loader &model, MyTexture *textures, vec3d pos, vec3d rotate, float scale) {
		glPushMatrix();
		glRotatef(rotate.x, 1, 0, 0); 	glRotatef(rotate.y, 0, 1, 0); glRotatef(rotate.z, 0, 0, 1); 
		glTranslated(pos.x, pos.y, pos.z);
		glScaled(scale, scale, scale);
		for (int i = 0; i < model.LoadedMeshes.size(); i++)
		{
			textures[i].beginTexture();
			glBegin(GL_TRIANGLES);
			for (int j = 0; j < model.LoadedMeshes[i].Vertices.size(); j++)
			{
				glTexCoord2d(
					model.LoadedMeshes[i].Vertices[j].TextureCoordinate.X,
					-model.LoadedMeshes[i].Vertices[j].TextureCoordinate.Y
				);
				glVertex3d(
					model.LoadedMeshes[i].Vertices[j].Position.X,
					model.LoadedMeshes[i].Vertices[j].Position.Y,
					model.LoadedMeshes[i].Vertices[j].Position.Z
				);
			}
			glEnd();
			textures[i].endTexture();
		}
		glPopMatrix();
	}
};


