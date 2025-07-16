// Simulation parameters using the data in mxsage's 36 Points,
// For each point, using 14 parameters from 21 in the original matrix

// A new parameter has been added at the end: it's some "sensing scaling factor".
// This is some tuning to make the original parameters produce better results with the new simulation method.

const int NumberOfBasePoints = 24;
const int PARAMS_DIMENSION = 15;

// default exponent when amplitude is 0, to tune for different mixing of Points (sensor distance) (see explanation comments after the matrix)
const double DE_SD = 2.000;
const double DE_SA = 1.000;
const double DE_RA = 1.000;
const double DE_MD = 3.000;

const double ParametersMatrix[NumberOfBasePoints][PARAMS_DIMENSION] = {
// // SD0 // SDE // SDA // SA0 // SAE // SAA // RA0 // RAE // RAA // MD0 // MDE // MDA // SB1 // SB2 // SF // see explanation below
    {0.000, 4.000, 0.300, 0.100, 51.32, 20.00, 0.410, 4.000, 0.000, 0.100, 6.000, 0.100, 0.000, 0.000, 22.0}, // 0 <-- A "pure_multiscale"
    {0.000, 28.04, 14.53, 0.090, DE_SA, 0.000, 0.010, 1.400, 1.120, 0.830, DE_MD, 0.000, 0.570, 0.030, 36.0}, // 1 <-- H "hex_hole_open"
    {17.92, DE_SD, 0.000, 0.520, DE_SA, 0.000, 0.180, DE_RA, 0.000, 0.100, 6.050, 0.170, 0.000, 0.000, 18.0}, // 2 <-- C "vertebrata"
    {3.000, 10.17, 0.400, 1.030, 2.300, 2.000, 1.420, 20.00, 0.750, 0.830, 1.560, 0.110, 1.070, 0.000, 13.0}, // 3 <-- E "star_network"
    {0.000, 8.510, 0.190, 0.610, DE_SA, 0.000, 3.350, DE_RA, 0.000, 0.750, 12.62, 0.060, 0.000, 0.000, 34.0}, // 4 <-- F "enmeshed_singularities"
    {0.000, 0.820, 0.030, 0.180, DE_SA, 0.000, 0.260, DE_RA, 0.000, 0.000, 20.00, 0.650, 0.200, 0.900, 31.5}, // 5 <-- B "waves_upturn"
    {1.500, 1.940, 0.280, 1.730, 1.120, 0.710, 0.180, 2.220, 0.850, 0.500, 4.130, 0.110, 1.120, 0.000, 15.0}, // 6 <-- G "more_individuals"
    {2.870, 3.040, 0.280, 0.090, DE_SA, 0.000, 0.440, 0.850, 0.000, 0.000, 2.220, 0.140, 0.300, 0.850, 11.0}, // 7 <-- I "sloppy_bucky"
    {0.140, 1.120, 0.190, 0.270, 1.400, 0.000, 1.130, 2.000, 0.390, 0.750, 2.220, 0.190, 0.000, 7.140, 9.00}, // 8 <-- J "massive_structure"
    {0.001, 2.540, 0.080, 0.000, DE_SA, 0.000, 3.350, DE_RA, 0.000, 0.100, 12.62, 0.060, 0.000, 0.000, 30.5}, // 9 <-- K "speed_modulation"
    {0.000, 28.04, 20.00, 0.180, 26.74, 20.00, 0.010, 1.400, 1.120, 0.830, DE_MD, 0.000, 2.540, 0.000, 39.0}, // 10 <-- L "transmission_tower"
    {0.000, 20.00, 3.000, 0.260, 2.150, 4.760, 0.410, 6.600, 12.62, 0.300, 6.600, 0.037, 0.400, 0.040, 28.0}, // 11 <-- M "ink_on_white"
    {27.50, 2.000, 2.540, 0.880, 26.74, 0.000, 0.090, 2.000, 1.400, 0.100, 5.000, 7.410, 1.400, 14.25, 12.0}, // 12 <-- N "vanishing_points"
    {0.000, 6.000, 100.0, 0.157, 1.000, 1.070, 0.000, 1.000, 5.000, 0.830, 5.000, 20.00, 0.400, 0.000, 8.00}, // 13 <-- Q "scaling_nodule_emergence"
    {0.000, 15.00, 8.600, 0.030, DE_SA, 0.000, 0.340, 2.000, 1.070, 0.220, 15.00, 0.100, 2.300, 0.820, 38.0}, // 14 <-- O "hyp_offset"
    {0.000, 32.88, 402.0, 0.410, 3.000, 0.000, 0.100, DE_RA, 0.000, 0.300, 6.000, 0.000, 0.000, 0.000, 32.0}, // 15 <-- D "strike"
    {0.000, 0.800, 0.020, 5.200, DE_SA, 0.000, 0.260, 0.100, 2.790, 0.830, 32.88, 37.74, 0.090, 0.330, 22.0}, // 16 <-- P "clear_spaghetti"
    {3.000, 10.17, 0.400, 1.030, 0.308, 0.000, 0.148, 20.00, 0.750, 0.830, 1.560, 0.110, 1.070, 0.040, 9.00}, // 17 <-- R
    {0.000, 5.000, 0.050, 0.900, 2.800, 0.000, 0.006, 0.840, 1.110, 0.750, 1.200, 0.000, 0.000, 0.000, 21.0}, // 18 <-- S
    {27.50, 28.04, 0.000, 0.390, 1.400, 0.000, 0.090, 0.846, 1.400, 0.100, 2.031, 0.070, 1.400, 0.030, 15.3}, // 19 <-- T
    {0.000, 8.500, 0.029, 0.270, 0.000, 0.000, 0.410, 0.000, 0.000, 0.750, 12.62, 0.060, 0.840, 0.000, 31.8}, // 20 <-- U
    {0.000, 6.370, 5.425, 1.030, 0.000, 0.000, 0.180, 0.289, 0.443, 0.300, 2.200, 0.065, 1.070, 0.040, 19.0}, // 21 <-- V
    {1.464, 20.00, 80.00, 0.260, 2.150, 4.760, 1.513, 2.000, 12.62, 0.385, 12.62, 0.037, 1.000, 0.000, 25.0}, //
    {0.000, 6.000, 100.0, 0.650, 0.175, 1.284, 0.000, 0.600, 5.000, 0.830, 5.395, 20.00, 0.400, 0.000, 8.60}, //
};

// The parameters of the classic physarum algorithm are sensor distance, sensor angle, rotation angle and move distance.
// In the 36 Points algorithm, we first sense a value s at particle position, or near it (with parameters SB1 and SB2)
// Then we set the classic parameters using the 36 Points parameters:
// sensor distance = SD0 + SDA * pow(s,SDE)
// sensor angle = SA0 + SAA * pow(s,SAE)
// rotation angle = RA0 + RAA * pow(s,RAE)
// move distance = MD0 + MDA * pow(s,MDE)

// See https://cargocollective.com/sagejenson/physarum
// (for classic algorithm and other information)
