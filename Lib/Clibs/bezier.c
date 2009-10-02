/*
 Code to generate a cubic Bezier curve
 From wikipedia -- the free encyclopedia
*/

typedef struct {
	double x;
	double y;
} Point2D;

static inline Point2D PointOnCubicBezier (Point2D* cp, double t)
{
	double ax, bx, cx;
	double ay, by, cy;
	double tSquared, tCubed;
	Point2D result;

	/* calculate the polynomial coefficients */

	cx = 3.0 * (cp [1].x - cp [0].x);
	bx = 3.0 * (cp [2].x - cp [1].x) - cx;
	ax = cp [3].x - cp [0].x - cx - bx;

	cy = 3.0 * (cp [1].y - cp [0].y);
	by = 3.0 * (cp [2].y - cp [1].y) - cy;
	ay = cp [3].y - cp [0].y - cy - by;

	/* calculate the curve point at parameter value t */

	tSquared = t * t;
	tCubed = tSquared * t;

	result.x = (ax * tCubed) + (bx * tSquared) + (cx * t) + cp [0].x;
	result.y = (ay * tCubed) + (by * tSquared) + (cy * t) + cp [0].y;

	return result;
}

/*
cubic_bezier fills an array of Point2D structs with the curve  points
generated from the control points cp. Caller must allocate sufficient memory
for the result, which is <sizeof(Point2D) * numberOfPoints>
*/

void cubic_bezier (int x0, int y0, int x1, int y1, int x2,
	int y2, int x3, int y3, int numberOfPoints, int *curve)
{
	Point2D r;
	double dt;
	int i, j;
	Point2D cp [] = { {x0, y0}, {x1, y1}, {x2, y2}, {x3, y3} };

	dt = 1.0 / (numberOfPoints - 1);

	for (j = i = 0; i < numberOfPoints; i++) {
		r = PointOnCubicBezier (cp, i * dt);
		curve [j++] = r.x;
		curve [j++] = r.y;
	}
}

/*
quadratic beziers
*/

static inline Point2D PointOnConicBezier (Point2D* cp, double t)
{
	double t1 = 1 - t;
	double tm = 2 * t * t1;
	Point2D result;

	t *= t;
	t1 *= t1;

	result.x = t1 * cp [0].x + tm * cp [1].x + t * cp [2].x;
	result.y = t1 * cp [0].y + tm * cp [1].y + t * cp [2].y;

	return result;
}

void conic_bezier (int x0, int y0, int x1, int y1, int x2,
	int y2, int numberOfPoints, int *curve)
{
	Point2D r;
	double dt;
	int i, j;
	Point2D cp [] = { {x0, y0}, {x1, y1}, {x2, y2} };

	dt = 1.0 / (numberOfPoints - 1);

	for (j = i = 0; i < numberOfPoints; i++) {
		r = PointOnConicBezier (cp, i * dt);
		curve [j++] = r.x;
		curve [j++] = r.y;
	}
}

const char ABI [] =
"- cubic_bezier		iiiiiiiiip32	\n"
"- conic_bezier		iiiiiiip32	\n"
;
