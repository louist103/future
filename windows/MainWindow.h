#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "WindowBase.h"

class MainWindow : public WindowBase {
public:
	MainWindow() {};
	~MainWindow() {};
	void DrawWindow() override;
};

#endif //MAINWINDOW_H
