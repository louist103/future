#ifndef WINDOWBASE_H
#define WINDOWBASE_H

class WindowBase {
public:
	WindowBase() {};
	virtual ~WindowBase() {};
	virtual void DrawWindow() = 0;
};


#endif
