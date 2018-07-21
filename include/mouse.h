//%%%%%
//% #pragma src "/sys/src/libdraw"

#if (!defined(__MOUSE_H__))
#define __MOUSE_H__

#include <l4all.h>
#include <l_actobj.h>

//% typedef struct	Channel Channel;  // thread.h

typedef struct	Cursor Cursor;
typedef struct	Menu Menu;
typedef struct 	Mousectl Mousectl;
//typedef struct	Mouse Mouse;

struct	Mouse
{
	int	buttons;	/* bit array: LMR=124 */
	Point	xy;
	ulong	msec;
};

struct Mousectl
{
	L_thcb  inherit_thcb;	// HK 20091130
       Mouse   _mouse;  //%
	L_thcb *mbox;	/* chan(Mouse) */		// HK 20091231
	L_thcb *resizec;	/* chan(int)[2] */	// HK 20091130
			/* buffered in case client is waiting for a mouse action before handling resize */

	char		*file;
	int		mfd;		/* to mouse file */
	int		cfd;		/* to cursor file */
	int		pid;		/* of slave proc */
	Image*	image;	/* of associated window/display */
};

struct Menu
{
	char	**item;
	char	*(*gen)(int);
	int	lasthit;
};

/*
 * Mouse
 */
extern Mousectl*	initmouse(char*, Image*);
extern void		moveto(Mousectl*, Point);
extern int			readmouse(Mousectl*);
extern void		closemouse(Mousectl*);
extern void		setcursor(Mousectl*, Cursor*);
extern void		drawgetrect(Rectangle, int);
extern Rectangle	getrect(int, Mousectl*);
extern int	 		menuhit(int, Mousectl*, Menu*, Screen*);

#endif		// __MOUSE_H__

