/*--
	micrOMEGAs for Colored Dark Sectors

	The code add Sommerfeld corrections for annihilation of colored particles.
	These colored particles are identified by their PDG code, see the code for
	more details. This code works with a FeynRules model file shipped with 
	the paper arXiv:1701.abcde.
--*/


#include "../include/micromegas.h"
#include "../include/micromegas_aux.h"
#include "lib/pmodel.h"
#include "stdbool.h"


// Variable which sets sommerfeld enhancement on or off.
static bool sommerfeld_on;

// Helper functions.
long color(long pdg);
long spin(long pdg);
double invexp(double x);
double alpha_strong(double q);

// Cross section functions.
double xx_to_qq(double alpha_s, double alpha_sommerfeld, int rep, int spin, double m, double v, bool sommerfeld);
double xx_to_gg(double alpha_s, double alpha_sommerfeld, int rep, int spin, double m, double v, bool sommerfeld);

double ss_to_qq(double alpha_s, double alpha_sommerfeld, int rep, double m, double v);
double ff_to_qq(double alpha_s, double alpha_sommerfeld, int rep, double m, double v);
double vv_to_qq(double alpha_s, double alpha_sommerfeld, int rep, double m, double v);

double ss_to_gg(double alpha_s, double alpha_sommerfeld, int rep, double m, double v);
double ff_to_gg(double alpha_s, double alpha_sommerfeld, int rep, double m, double v);
double vv_to_gg(double alpha_s, double alpha_sommerfeld, int rep, double m, double v);

double ss_to_qq_sommerfeld(double alpha_s, double alpha_sommerfeld, int rep, double m, double v);
double ff_to_qq_sommerfeld(double alpha_s, double alpha_sommerfeld, int rep, double m, double v);
double vv_to_qq_sommerfeld(double alpha_s, double alpha_sommerfeld, int rep, double m, double v);

double ss_to_gg_sommerfeld(double alpha_s, double alpha_sommerfeld, int rep, double m, double v);
double ff_to_gg_sommerfeld(double alpha_s, double alpha_sommerfeld, int rep, double m, double v);
double vv_to_gg_sommerfeld(double alpha_s, double alpha_sommerfeld, int rep, double m, double v);


/*-- Main Program --*/

// Main part of the program.
int main(int argc, char** argv)
{
	int err;
	char cdmName[10];
	int spin2, charge3, cdim;
	ForceUG = 0;  /* to Force Unitary Gauge assign 1 */

	if (argc == 1)
	{ 
		printf("Correct usage: ./main <file with parameters> <sommerfeld>\n");
		printf("Example: ./main data1.par\n");
		exit(1);
	}

	// Determine if sommerfeld enhancement is enabled.
	sommerfeld_on = !!argv[2];
	printf("Sommerfeld corrections enabled: %s\n", sommerfeld_on ? "true" : "false");

	// Read in parameter file.
	err = readVar(argv[1]);
	if (err == -1)
	{
		printf("Can not open the file\n");
		exit(1);
	}
	else if (err > 0)
	{
		printf("Wrong file contents at line %d\n", err);
		exit(1);
	}

	err = sortOddParticles(cdmName);
	if (err)
	{
		printf("Can't calculate %s\n", cdmName);
		return 1;
	}

	if (CDM1) 
	{ 
		qNumbers(CDM1, &spin2, &charge3, &cdim);
		printf("\nDark matter candidate is '%s' with spin=%d/2 mass=%.2E\n", CDM1, spin2, Mcdm1); 
		if (charge3)
			printf("Dark Matter has electric charge %d/3\n", charge3);
		if (cdim != 1)
			printf("Dark Matter is a color particle\n");
	}
	if (CDM2) 
	{ 
		qNumbers(CDM2, &spin2, &charge3, &cdim);
		printf("\nDark matter candidate is '%s' with spin=%d/2 mass=%.2E\n", CDM2, spin2, Mcdm2); 
		if (charge3)
			printf("Dark Matter has electric charge %d/3\n", charge3);
		if (cdim != 1)
			printf("Dark Matter is a color particle\n");
	}

	printMasses(stdout, 1);

	int fast = 0;
	double Beps = 1.E-7;
	double cut = 0.0001;
	double Omega, OmegaFO;
	printf("\n==== Calculation of relic density =====\n");   

	double Xf, XfFO;
	Omega = darkOmega(&Xf, fast, Beps);
	OmegaFO = darkOmegaFO(&XfFO, fast, Beps);

	printf("Xf=%.4e Omega=%.4e\n", Xf, Omega);
	printf("Xf(FO)=%.4e Omega(FO)=%.4e\n", XfFO, OmegaFO);
	printChannels(XfFO, cut, Beps, 1, stdout);
	printf("omega_h^2 = %.4E\n", Omega);
	printf("omega_h^2(FO) = %.4E\n", OmegaFO);

	killPlots();
	return 0;
}


/*-- Cross Section Improvement --*/

void improveCrossSection(long n1, long n2, long n3, long n4, double pin, double *res)
{
	// Return zero for all process which do not have two equal colored X's.
	if (abs(n1) < 9000000 || abs(n2) < 9000000 || abs(n1) != abs(n2) || color(n1) < 3 || color(n2) < 3)
	{
		printf("WARNING: process %d %d -> %d %d is being ignored\n", (int)n1, (int)n2, (int)n3, (int)n4);
		*res = 0;
		return;	
	}

	// Get incoming particle masses.
	double m1 = pMass(pdg2name(n1));
	double m2 = pMass(pdg2name(n2));
	double m = (m1 + m2) / 2.0;
	if (m1 != m2)
		printf("WARNING: masses of incoming particles are are not equal: %f and %f", m1, m2);

	// Calculate beta and s.
	double v = pin / sqrt(pow(pin, 2.0) + pow(m1, 2.0));
	double s = pow(sqrt(pow(pin, 2.0) + pow(m1, 2.0)) + sqrt(pow(pin, 2.0) + pow(m2, 2.0)), 2.0);

	// Calculate alpha_sommerfeld at the scale of the soft gluons.
	double alpha_sommerfeld = alpha_strong(pin);
	// micrOMEGAs uses its own running for the hard process.
	double alpha_mo = parton_alpha(GGscale);

	// Determine color and spin of X.
	long color_x = color(n1);
	long spin_x = spin(n1);
	if (color_x != 3 && color_x != 6 && color_x != 8)
		printf("color of x is invalid: %d", (int)color_x);
	if (spin_x < 1 || spin_x > 6)
		printf("spin of x is invalid: %d", (int)spin_x);

	// Add sommerfeld factor for XX -> qq.
	if (abs(n3) >= 1 && abs(n3) <= 6 && abs(n4) >= 1 && abs(n4) <= 6)
	{
		double xsec_mo = *res;
		double xsec = xx_to_qq(alpha_mo, alpha_sommerfeld, color_x, spin_x, m, v, sommerfeld_on);
		// Safety check: xsec is not a number.
		if (!isfinite(xsec) || isnan(xsec))
		{
			printf("WARNING: xsec not a number (%f) for %d %d -> %d %d\n", xsec, (int)n1, (int)n2, (int)n3, (int)n4);
			printf("\tmass: %f, s: %f, v: %f, p: %f, alpha_s: %f, alpha_sommerfeld: %f\n", m, sqrt(s), v, pin, alpha_mo, alpha_sommerfeld);
		}
		// Safety check: micromegas vs. analytic cross section (0.1% agreement needed). 
		if (!sommerfeld_on && fabs(1000 * (xsec - xsec_mo) / xsec_mo) > 1)
		{
			printf("WARNING: xsec mismatch to analytic for %d %d -> %d %d\n", (int)n1, (int)n2, (int)n3, (int)n4);
			printf("\tmass: %f, s: %f, v: %f, p: %f, alpha_s: %f, alpha_sommerfeld: %f\n", m, sqrt(s), v, pin, alpha_mo, alpha_sommerfeld);
			printf("\txsec(mo): %.8e, xsec(analytic): %.8e, ratio(mo/analytic): %.6f\n", xsec_mo, xsec, xsec_mo / xsec);
		}
		*res = xsec;
		return;
	}

	// Add sommerfeld factor for XX -> gg.
	if (n3 == 21 && n4 == 21)
	{
		double xsec_mo = *res;
		double xsec = xx_to_gg(alpha_mo, alpha_sommerfeld, color_x, spin_x, m, v, sommerfeld_on);
		// Safety check: xsec is not a number.
		if (!isfinite(xsec) || isnan(xsec))
		{
			printf("WARNING: xsec not a number (%f) for %d %d -> %d %d\n", xsec, (int)n1, (int)n2, (int)n3, (int)n4);
			printf("\tmass: %f, s: %f, v: %f, p: %f, alpha_s: %f, alpha_sommerfeld: %f\n", m, sqrt(s), v, pin, alpha_mo, alpha_sommerfeld);
		}
		// Safety check: micromegas vs. analytic cross section (0.1% agreement needed). 
		if (!sommerfeld_on && fabs(1000 * (xsec - xsec_mo) / xsec_mo) > 1)
		{
			printf("WARNING: xsec mismatch to analytic for %d %d -> %d %d\n", (int)n1, (int)n2, (int)n3, (int)n4);
			printf("\tmass: %f, s: %f, v: %f, p: %f, alpha_s: %f, alpha_sommerfeld: %f\n", m, sqrt(s), v, pin, alpha_mo, alpha_sommerfeld);
			printf("\txsec(mo): %.8e, xsec(analytic): %.8e, ratio(mo/analytic): %.6f\n", xsec_mo, xsec, xsec_mo / xsec);
		}
		*res = xsec;
		return;
	}

	// We only allow the channels XX -> gg and XX -> qq, in other cases the cross section is zero.
	printf("in: %d %d, out: %d %d set to zero\n", (int)n1, (int)n2, (int)n3, (int)n4);
	*res = 0;
	return; 
}


/*-- Helpers --*/

long color(long pdg)
{
	// The color is coded in the last digit of the PDG number.
	return abs(pdg) % 100;
}

long spin(long pdg)
{
	// The spin is coded in the 3rd to last digit of the PDG number.
	return abs(pdg / 100) % 100;
}

double invexp(double x)
{
	// This function is defined to prevent numerical issues for low x.	
	return exp(x) / (exp(x) - 1);
}

// See the Mathematica notebook for the details of this definition of alpha_strong.
double alpha_strong(double q)
{
	// Implement a cut off for the momentum q of 1 GeV to not enter the non-perturbative regime.
	q = fmax(q, 1.0);
	// MSbar masses for the quarks.
	double mtop = 160;
	double mbottom = 4.18;
	double mcharm = 1.28;
   	// Determine number of active flavors.
	double nf = 6.0;	
	if (q < mcharm) nf = 3.0;
	else if (q < mbottom) nf = 4.0;
	else if (q < mtop) nf = 5.0;
	// Determine the threshold lambdas.
	double lambda = 0.08896768177299201;
	if (q < mcharm) lambda = 0.33348050663724466;
	else if (q < mbottom) lambda = 0.2913885366061117;
	else if (q < mtop) lambda = 0.20953346238097081;
	// Determine alpha_strong and return it.
	double t = log(pow(q / lambda, 2.0));
	double z3 = 1.202056903159594;
	double b0 = (33.0 - 2.0 * nf) / (12.0 * M_PI);
	double b1 = (153.0 - 19.0 * nf) / (24.0 * pow(M_PI, 2.0));
	double b2 = (2857.0 - 5033.0 / 9.0 * nf + 325.0 / 27.0 * pow(nf, 2.0)) / (128.0 * pow(M_PI, 3.0));
	double b3 = ((149753.0 / 6.0 + 3564.0 * z3) - (1078361.0 / 162.0 + 6508.0 / 27.0 * z3) * nf + (50065.0 / 162.0 + 6472.0 / 81.0 * z3) * pow(nf, 2.0) + 1093.0 / 729.0 * pow(nf, 3.0)) / (256.0 * pow(M_PI, 4.0));
	return 1.0 / (b0 * t) * (1.0 - b1 / pow(b0, 2.0) * log(t) / t + (pow(b1, 2.0) * (pow(log(t), 2.0) - log(t) - 1.0) + b0 * b2) / (pow(b0, 4.0) * pow(t, 2.0)) - 1.0 / (pow(b0, 6.0) * pow(t, 3.0)) * (pow(b1, 3.0) * (pow(log(t), 3.0) - 5.0 / 2.0 * pow(log(t), 2.0) - 2.0 * log(t) + 1.0 / 2.0) + 3.0 * b0 * b1 * b2 * log(t) - 0.5 * pow(b0, 2.0) * b3));
}


/*-- Cross Sections --*/

double xx_to_qq(double alpha_s, double alpha_sommerfeld, int rep, int spin, double m, double v, bool sommerfeld)
{
	if (!sommerfeld)
	{
		switch (spin)
		{
			case 1: case 2: return ss_to_qq(alpha_s, alpha_sommerfeld, rep, m, v);
			case 3: case 4: return ff_to_qq(alpha_s, alpha_sommerfeld, rep, m, v);
			case 5: case 6: return vv_to_qq(alpha_s, alpha_sommerfeld, rep, m, v);
			default: printf("WARNING: xx_to_qq called for invalid spin %d.\n", spin); return 0.0;
		}
	}
	else
	{
		switch (spin)
		{
			case 1: case 2: return ss_to_qq_sommerfeld(alpha_s, alpha_sommerfeld, rep, m, v);
			case 3: case 4: return ff_to_qq_sommerfeld(alpha_s, alpha_sommerfeld, rep, m, v);
			case 5: case 6: return vv_to_qq_sommerfeld(alpha_s, alpha_sommerfeld, rep, m, v);
			default: printf("WARNING: xx_to_qq called for invalid spin %d.\n", spin); return 0.0;
		}
	}
	return 0.0;
}

double xx_to_gg(double alpha_s, double alpha_sommerfeld, int rep, int spin, double m, double v, bool sommerfeld)
{
	if (!sommerfeld)
	{
		switch (spin)
		{
			case 1: case 2: return ss_to_gg(alpha_s, alpha_sommerfeld, rep, m, v);
			case 3: case 4: return ff_to_gg(alpha_s, alpha_sommerfeld, rep, m, v);
			case 5: case 6: return vv_to_gg(alpha_s, alpha_sommerfeld, rep, m, v);
			default: printf("WARNING: gg_to_qq called for invalid spin %d.\n", spin); return 0.0;
		}
	}
	else
	{
		switch (spin)
		{
			case 1: case 2: return ss_to_gg_sommerfeld(alpha_s, alpha_sommerfeld, rep, m, v);
			case 3: case 4: return ff_to_gg_sommerfeld(alpha_s, alpha_sommerfeld, rep, m, v);
			case 5: case 6: return vv_to_gg_sommerfeld(alpha_s, alpha_sommerfeld, rep, m, v);
			default: printf("WARNING: gg_to_qq called for invalid spin %d.\n", spin); return 0.0;
		}
	}
	return 0.0;
}

double ss_to_qq(double alpha_s, double alpha_sommerfeld, int rep, double m, double v)
{
	switch (rep)
	{
		case 3: return ID_S3S3_QQ_NS;
		case 6: return ID_S6S6_QQ_NS;
		case 8: return ID_S8S8_QQ_NS;
		default: printf("WARNING: ss_to_qq called for invalid representation %d.\n", rep); return 0.0;
	}
	return 0.0;
}

double ff_to_qq(double alpha_s, double alpha_sommerfeld, int rep, double m, double v)
{
	switch (rep)
	{
		case 3: return ID_F3F3_QQ_NS;
		case 6: return ID_F6F6_QQ_NS;
		case 8: return ID_F8F8_QQ_NS;
		default: printf("WARNING: ff_to_qq called for invalid representation %d.\n", rep); return 0.0;
	}
	return 0.0;
}

double vv_to_qq(double alpha_s, double alpha_sommerfeld, int rep, double m, double v)
{
	switch (rep)
	{
		case 3: return ID_V3V3_QQ_NS;
		case 6: return ID_V6V6_QQ_NS;
		case 8: return ID_V8V8_QQ_NS;
		default: printf("WARNING: vv_to_qq called for invalid representation %d.\n", rep); return 0.0;
	}
	return 0.0;
}

double ss_to_gg(double alpha_s, double alpha_sommerfeld, int rep, double m, double v)
{
	switch (rep)
	{
		case 3: return ID_S3S3_GG_NS;
		case 6: return ID_S6S6_GG_NS;
		case 8: return ID_S8S8_GG_NS;
		default: printf("WARNING: vv_to_gg called for invalid representation %d.\n", rep); return 0.0;
	}
	return 0.0;
}

double ff_to_gg(double alpha_s, double alpha_sommerfeld, int rep, double m, double v)
{
	switch (rep)
	{
		case 3: return ID_F3F3_GG_NS;
		case 6: return ID_F6F6_GG_NS;
		case 8: return ID_F8F8_GG_NS;
		default: printf("WARNING: vv_to_gg called for invalid representation %d.\n", rep); return 0.0;
	}
	return 0.0;
}

double vv_to_gg(double alpha_s, double alpha_sommerfeld, int rep, double m, double v)
{
	switch (rep)
	{
		case 3: return ID_V3V3_GG_NS;
		case 6: return ID_V6V6_GG_NS;
		case 8: return ID_V8V8_GG_NS;
		default: printf("WARNING: vv_to_gg called for invalid representation %d.\n", rep); return 0.0;
	}
	return 0.0;
}

double ss_to_qq_sommerfeld(double alpha_s, double alpha_sommerfeld, int rep, double m, double v)
{
	switch (rep)
	{
		case 3: return ID_S3S3_QQ_SO;
		case 6: return ID_S6S6_QQ_SO;
		case 8: return ID_S8S8_QQ_SO;
		default: printf("WARNING: ss_to_qq_sommerfeld called for invalid representation %d.\n", rep); return 0.0;
	}
	return 0.0;
}

double ff_to_qq_sommerfeld(double alpha_s, double alpha_sommerfeld, int rep, double m, double v)
{
	switch (rep)
	{
		case 3: return ID_F3F3_QQ_SO;
		case 6: return ID_F6F6_QQ_SO;
		case 8: return ID_F8F8_QQ_SO;
		default: printf("WARNING: ff_to_qq_sommerfeld called for invalid representation %d.\n", rep); return 0.0;
	}
	return 0.0;
}

double vv_to_qq_sommerfeld(double alpha_s, double alpha_sommerfeld, int rep, double m, double v)
{
	switch (rep)
	{
		case 3: return ID_V3V3_QQ_SO;
		case 6: return ID_V6V6_QQ_SO;
		case 8: return ID_V8V8_QQ_SO;
		default: printf("WARNING: vv_to_qq_sommerfeld called for invalid representation %d.\n", rep); return 0.0;
	}
	return 0.0;
}

double ss_to_gg_sommerfeld(double alpha_s, double alpha_sommerfeld, int rep, double m, double v)
{
	switch (rep)
	{
		case 3: return ID_S3S3_GG_SO;
		case 6: return ID_S6S6_GG_SO;
		case 8: return ID_S8S8_GG_SO;
		default: printf("WARNING: ss_to_gg_sommerfeld called for invalid representation %d.\n", rep); return 0.0;
	}
	return 0.0;
}

double ff_to_gg_sommerfeld(double alpha_s, double alpha_sommerfeld, int rep, double m, double v)
{
	switch (rep)
	{
		case 3: return ID_F3F3_GG_SO;
		case 6: return ID_F6F6_GG_SO;
		case 8: return ID_F8F8_GG_SO;
		default: printf("WARNING: ff_to_gg_sommerfeld called for invalid representation %d.\n", rep); return 0.0;
	}
	return 0.0;
}

double vv_to_gg_sommerfeld(double alpha_s, double alpha_sommerfeld, int rep, double m, double v)
{
	switch (rep)
	{
		case 3: return ID_V3V3_GG_SO;
		case 6: return ID_V6V6_GG_SO;
		case 8: return ID_V8V8_GG_SO;
		default: printf("WARNING: vv_to_gg_sommerfeld called for invalid representation %d.\n", rep); return 0.0;
	}
	return 0.0;
}

