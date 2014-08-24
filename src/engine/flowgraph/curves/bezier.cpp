/**
* Include bezier curve header file
* and Sauerbraten engine file
*/
#include "engine.h"
#include "bezier.h"


/**
* Constructor
*/
CBezierCurve::CBezierCurve() {
	// curve not computed in the beginning
	m_bComputed = false;
	// set default precision
	m_fCalcPrecision = 30.0f; // 1.0f up to 20.0f ?
	// set parameter point limit
	m_uiParamLimit = 7;
}

/**
* Destructor
*/
CBezierCurve::~CBezierCurve() {
	// nothing to destruct (yet)
}

/**
* Try to add parameter point to curve
*/
void CBezierCurve::AddParamPoint(SPoint point) 
{
	// check if parameter point limit was reached or not
	if(m_ParameterPoints.size() < m_uiParamLimit) {
		// push back (= add) parameter point
		m_ParameterPoints.push_back( point);
	}
}

void CBezierCurve::AddParamPoint(float x, float y, float z) 
{
	// check if parameter point limit was reached or not
	if(m_ParameterPoints.size() < m_uiParamLimit) 
	{
		// add point manually
		SPoint point;
		point.x = x;
		point.y = y;
		point.z = z;
		// push back (= add) parameter point
		m_ParameterPoints.push_back( point);
	}
}


// binomial coefficient for bernstein polynom
unsigned int CBezierCurve::binomialCoef(unsigned int n, const unsigned int k)
{
	unsigned int r = 1;
	if(k > n) return 0;
	for(unsigned int d = 1; d <= k; d++) {
		r *= n--;
		r /= d;
	}
	return r;
}


/**
* Calculate (Method 1)
*/
void CBezierCurve::CalculateCurve_BernsteinPolynom(void) 
{
	// Calculate precision
	float fStep = 1.0f / m_fCalcPrecision;

	// debug messages
	#define CURVE_RENDERER_DEBUG_MODE
	#ifdef CURVE_RENDERER_DEBUG_MODE
		conoutf(CON_DEBUG, "Precision: %f", fStep);
	#endif

	// the number of parameter points given
	int uiElements = m_ParameterPoints.size() - 1;

	// go along our curve in fPos steps
	for( float fPos=0.0f;  fPos <= 1.0f;  fPos+=fStep) 
	{
		// computed point
		SPoint finished_point;
		finished_point.x = 0.0f;
		finished_point.y = 0.0f;
		finished_point.z = 0.0f;
					
		// apply fPos to our bernstein polynom					
		for( int i=0; i<=uiElements; i++) 
		{
			// compute bezier coordinates using bernstein polynoms
			finished_point.x += binomialCoef( uiElements, i) * pow( fPos, i) * pow(  (1-fPos),  (uiElements-i) ) * m_ParameterPoints[ i ].x;
			finished_point.y += binomialCoef( uiElements, i) * pow( fPos, i) * pow(  (1-fPos),  (uiElements-i) ) * m_ParameterPoints[ i ].y;
			finished_point.z += binomialCoef( uiElements, i) * pow( fPos, i) * pow(  (1-fPos),  (uiElements-i) ) * m_ParameterPoints[ i ].z;
		}

		// debug messages
		#ifdef CURVE_RENDERER_DEBUG_MODE
			conoutf(CON_DEBUG, "Output position: %f %f %f", finished_point.x,finished_point.y,finished_point.z);
		#endif

		// add computed point
		m_ComputedPoints.push_back(finished_point);
	}
}

/**
* Calculate (Method 2)
*/
void CBezierCurve::CalculateCurve_DeCasteljauRecursive(void) {

}

// set parameter point limit
void CBezierCurve::SetParamPointLimit(unsigned int limit) {
	// set limit
	m_uiParamLimit = limit;
}

// set final curve points' precision
// We should not compute it in real time...
void CBezierCurve::SetPrecision(float precision) {
	// set precision
	m_fCalcPrecision = precision;
}