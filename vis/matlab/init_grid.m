function [Grid,status] = init_grid(filename)
% 
% init_grid:  INITIALIZES A GRID STRUCTURE BY OPENING A .bin FILE (NOT
% NECESSARILY CYCLE 0) AND READING IN HEADER INFORMATION ONLY.  SOME EXTRA
% VARIABLES ARE COMPUTED FOR CLARITY.  
%
% AUTHOR:  AARON SKINNER
% LAST MODIFIED:  6/23/09

status = 0;

% PARSE FILENAME AND TEST TO SEE IF .bin
[path,basename,step,ext] = parse_filename(filename);
if (~strcmp(ext,'.bin'))
    fprintf(2,'[init_grid]:  %s is not a .bin file!\n', filename);
    status = -1;
    return;
end;

% OPEN FILE FOR READING
[fid, message] = fopen(filename,'r');
if (fid==-1)
    fprintf(2,'[init_grid]:  %s could not be opened!\n', filename);
    fprintf(2,'%s', message);
    status = -1;
    return;
end;

% READ COORDINATE SYSTEM INFORMATION
coordsys = fread(fid,1,'int');

% READ NUMBER OF DATA POINTS, VARIABLES, SCALARS, ETC.
dat = fread(fid,6,'int');
nx1      = dat(1);
nx2      = dat(2);
nx3      = dat(3);
nvar     = dat(4);
nscalars = dat(5);
ifgrav   = dat(6);
ndim     = (nx1 > 1) + (nx2 > 1) + (nx3 > 1);
if (~(ndim==1 || ndim==2 || ndim==3))
    fprintf(2,'[init_grid]:  %d is an invalid dimension!\n', ndims);
    status = -1;
    return;
end;
if (~((nvar==4) || (nvar==5) || (nvar==7) || (nvar==8)))
    fprintf(2,'[init_grid]:  %d is an invalid number of variables!\n', nvar);
    status = -1;
    return;
end;

% READ (Gamma-1), ISOTHERMAL SOUND SPEED, TIME, AND dt
dat = fread(fid,4,'float');
gamma_1     = dat(1);
iso_csound  = dat(2);
time_offset = ftell(fid);  % GET POSITION OF time, dt
time        = dat(3);  % READ IN, BUT NOT USED
dt          = dat(4);  % READ IN, BUT NOT USED

% READ X1,X2,X3 COORDINATES
x1zones = fread(fid,nx1,'float');
x2zones = fread(fid,nx2,'float');
x3zones = fread(fid,nx3,'float');
data_offset = ftell(fid);

% CLOSE FILE
status = fclose(fid);
if (status == -1)
    fprintf(2,'[init_grid]:  %s could not be closed!\n', filename);
end;

% COMPUTE SOME DERIVED QUANTITIES
dx1 = 0; dx2 = 0; dx3 = 0;
if (nx1>1) 
    dx1 = (max(x1zones)-min(x1zones))/(nx1-1);
end;
if (nx2>1) 
    dx2 = (max(x2zones)-min(x2zones))/(nx2-1);
end;
if (nx3>1) 
    dx3 = (max(x3zones)-min(x3zones))/(nx3-1);
end;
% X1MIN, X1MAX, ETC. ARE THE ABSOLUTE LIMITS OF THE GRID
x1min = min(x1zones) - 0.5*dx1;  x1max = max(x1zones) + 0.5*dx1;
x2min = min(x2zones) - 0.5*dx2;  x2max = max(x2zones) + 0.5*dx2;
x3min = min(x3zones) - 0.5*dx3;  x3max = max(x3zones) + 0.5*dx3;
x1nodes = linspace(x1min,x1max,nx1+1);
x2nodes = linspace(x2min,x2max,nx2+1);
x3nodes = linspace(x3min,x3max,nx3+1);

% INITIALIZE GRID STRUCTURE
Grid.coordsys    = coordsys;
Grid.nx1         = nx1;
Grid.nx2         = nx2;
Grid.nx3         = nx3;
Grid.dx1         = dx1;
Grid.dx2         = dx2;
Grid.dx3         = dx3;
Grid.x1min       = x1min;
Grid.x1max       = x1max;
Grid.x2min       = x2min;
Grid.x2max       = x2max;
Grid.x3min       = x3min;
Grid.x3max       = x3max;
Grid.ndim        = ndim;
Grid.nvar        = nvar;
Grid.nscalars    = nscalars;
Grid.gamma_1     = gamma_1;
Grid.iso_csound  = iso_csound;
Grid.gravity     = (ifgrav == 1);
Grid.adiabatic   = (nvar==5 || nvar==8);
Grid.mhd         = (nvar==7 || nvar==8);
Grid.x1zones     = x1zones;
Grid.x2zones     = x2zones;
Grid.x3zones     = x3zones;
Grid.x1nodes     = x1nodes;
Grid.x2nodes     = x2nodes;
Grid.x3nodes     = x3nodes;
Grid.time_offset = time_offset;
Grid.data_offset = data_offset;

return;