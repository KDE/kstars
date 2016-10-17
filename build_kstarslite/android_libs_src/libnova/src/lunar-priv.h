/* used for elp1 - 3 */
struct main_problem
{
	double ilu[4];
	double A;
	double B[6];
};

/* used for elp 4 - 9 */
struct earth_pert
{
	double iz;
	double ilu[4];
	double O;
	double A;
	double P;
}; 

/* used for elp 10 - 21 */
struct planet_pert
{
	double ipla[11];
	double theta;
	double O;
	double P;
};


