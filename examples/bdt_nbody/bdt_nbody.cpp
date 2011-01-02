/* bdt_nbody.cpp
 *
 * Copyright (c) 2008-2009 Brown Deer Technology, LLC.  All Rights Reserved.
 *
 * This software was developed by Brown Deer Technology, LLC.
 * For more information contact info@browndeertechnology.com
 * 
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/* DAR */

#include <algorithm>
#include <cstring>
#include <math.h> 
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <sys/time.h>

#include<GL/glut.h>

#ifdef ENABLE_CL
#include<CL/cl.h>
#include "stdcl.h"
#include "nbody_kern.clh"
#endif

#include "Timer.h"
#include "nbody.h"
#include "nbody_display.h"


CONTEXT* cp = 0; /* selects CL context */

void (*iterate)(
   int nburst, int nparticle, int nthread,
   cl_float gdt, cl_float es,
   cl_float* pp, cl_float* vv
) = 0;

struct iterate_args_struct iterate_args = __init_iterate_args();

char* cldevstr;

int step_count = 0;
float gflops;

cl_kernel k_nbody;

/*
 * main program
 */

int main(int argc, char** argv)
{
Setup(0);
	int nparticle = DEFAULT_NPARTICLE;

	int i,j,k;
	int step = 0;
	int n;

	int nstep = DEFAULT_NSTEP;
	int nburst = DEFAULT_NBURST;
	int nthread = DEFAULT_NTHREAD;
	int nb = DEFAULT_NBLOCK;

	int use_cpu = 0;
//#ifdef ENABLE_BROOK
//	int use_gpu_br = 0;
//#endif
//#ifdef ENABLE_CL
//	int use_cpu_cl = 0;
//	int use_gpu_cl = 0;
//#endif

	int dump_results = 0;

	float stime = 0.0;
	float gdt = G_CONSTANT*DEFAULT_DT;
	float es = DEFAULT_ES;
	float time;
	float tmpx,tmpy,tmpz;

	float* pp = 0;
	float* vv = 0;

	FILE* fp;

	printf(
		"\nRunning bdt_nbody"
		" - a simple GPU-accelerated NBody Simulation.\n"
		"Copyright (c) 2008-2009 Brown Deer Technology, LLC. "
		" All Rights Reserved.\n"
		"This program is free software distributed under GPLv3.\n\n"
	);

	i = 1;
	while (i < argc) {

      if (!strcmp(argv[i],"--cpu")) use_cpu = 1;

#ifdef ENABLE_BROOK
      else if (!strcmp(argv[i],"--gpu-br")) use_gpu_br = 1;
#endif

#ifdef ENABLE_CL
//      else if (!strcmp(argv[i],"--cpu-cl")) use_cpu_cl = 1;
//      else if (!strcmp(argv[i],"--gpu-cl")) use_gpu_cl = 1;
      else if (!strcmp(argv[i],"--cl-device")) {
			++i;
			if (!strncmp(argv[i],"gpu",3)) cp = stdgpu;
			else if (!strncmp(argv[i],"cpu",3)) cp = stdcpu;
		}
#endif

		else if (!strcmp(argv[i],"--display")) ;
		else if (!strcmp(argv[i],"--nstep")) nstep = atoi(argv[++i]);
		else if (!strcmp(argv[i],"--nburst")) nburst = atoi(argv[++i]);
		else if (!strcmp(argv[i],"--nblock")) nb = atoi(argv[++i]);
		else if (!strcmp(argv[i],"--nthread")) nthread = atoi(argv[++i]);
		else if (!strcmp(argv[i],"--nparticle")) nparticle = atoi(argv[++i]);
		else if (!strcmp(argv[i],"-D")) dump_results = 1;
		else {
			fprintf(stderr,"unrecognized option: %s\n",argv[i]);
			exit(1);
		}
		++i;
	}

   if (nparticle%16) {
      fprintf( stderr, "nparticle must be multiple of 16, stopping.\n"); 
      exit(1);
   }

   if (nstep%nburst) {
      fprintf( stderr, "nstep must be multiple of nburst, stopping.\n"); 
      exit(1);
   }

	gdt = G_CONSTANT*DEFAULT_DT;

	printf(
		"nstep=%d nburst=%d nparticle=%d gdt=%e\n",
		nstep,nburst,nparticle,gdt
	);

#ifdef ENABLE_CL
	if (!cp) {
		if (clgetndev(stdgpu)) cp = stdgpu;
		else if (clgetndev(stdcpu)) cp = stdcpu;
	}
	if (cp) {
		pp = (cl_float*)clmalloc(cp,sizeof(cl_float)*4*nparticle,0);
		vv = (cl_float*)clmalloc(cp,sizeof(cl_float)*4*nparticle,0);
	}
#endif
	if (!pp) pp = (cl_float*)malloc(sizeof(cl_float)*4*nparticle);
	if (!vv) vv = (cl_float*)malloc(sizeof(cl_float)*4*nparticle);

	if (pp == 0 || vv == 0 ) {
		fprintf(stderr,"memory allocation failed.  Terminating...\n");
		exit(-1);
	}


#ifdef ENABLE_CL
   struct clstat_info stat_info;
   struct cldev_info* dev_info = (struct cldev_info*)malloc(clgetndev(cp)*sizeof(struct cldev_info));

   clstat(cp,&stat_info);
   clgetdevinfo(cp,dev_info);
	clfreport_devinfo(stdout,1,dev_info);

	cldevstr = (char*)malloc(64);
	if (dev_info->dev_type == CL_DEVICE_TYPE_GPU) {
		snprintf(cldevstr,64,"GPU/OpenCL %s",dev_info->dev_name);
	} else if (dev_info->dev_type == CL_DEVICE_TYPE_CPU) {
		snprintf(cldevstr,64,"CPU/OpenCL %s",dev_info->dev_name);
	} else strncpy(cldevstr,"???",3);

//	void* hcl = clopen(cp,0,0);
//	void* hcl = clopen(stdgpu,0,0);
	void* hcl = clopen(stdgpu,"nbody_kern.cl",0);
//	void* hcl = clopen(stdgpu,"NBody_Kernels.cl",0);
	k_nbody = clsym(stdgpu,hcl,"nbody_kern",0);
//exit(0);
#endif


	Setup(0);

   int use_display = 1;

   float s = 100.0f;

   nbody_init( nparticle, pp, vv );

	iterate_args.nburst = nburst; // 10;
	iterate_args.nparticle = nparticle;
	iterate_args.nthread = nthread;
	iterate_args.gdt = gdt;
	iterate_args.es = es;
	iterate_args.pp = pp;
	iterate_args.vv = vv;

	iterate = (cp)? iterate_cl : iterate_cpu;

   if(use_display) {
        // Run in  graphical window if requested

printf("argc=%d\n",argc);
for(i=0;i<argc;i++) printf("%s\n",argv[i]);
fflush(stdout);

        glutInit(&argc, argv);
        glutInitWindowPosition(100,10);
        glutInitWindowSize(600,600);
        glutInitDisplayMode( GLUT_RGB | GLUT_DOUBLE );
        glutCreateWindow("Brown Deer Technology nbody simulation");
        display_init();
        glutDisplayFunc(displayfunc);
        glutReshapeFunc(reShape);
        glutIdleFunc(idle);
        glutKeyboardFunc(keyboardFunc);
        glutMainLoop();
   }



cleanup:

#ifdef ENABLE_CL
	clclose(cp,hcl);
	if (pp) clfree(pp);
	if (vv) clfree(vv);
#else
	if (pp) free(pp);
	if (vv) free(vv);
#endif

   return(0);

}


void iterate_cpu(
   int nburst, int nparticle, int dummy,
   float gdt, float es,
   float* pp, float* vv
)
{

	Start(0);

	for(int burst = 0; burst<nburst; burst++) {

	for(int i=0; i< nparticle; i++) {

		float x = pp[__index_x(i)];
		float y = pp[__index_y(i)];
		float z = pp[__index_z(i)];
	
		float tmp = x;
	
		float vx = vv[__index_x(i)];
		float vy = vv[__index_y(i)];
		float vz = vv[__index_z(i)];
		
		float ax = 0.0;
		float ay = 0.0;
		float az = 0.0;

		for(int j=0; j<nparticle; j++) {

			float x2 = pp[__index_x(j)];
			float y2 = pp[__index_y(j)];
			float z2 = pp[__index_z(j)];
			float m2 = pp[__index_m(j)];
		
			float dx = x2 - x;
			float dy = y2 - y;
			float dz = z2 - z;
	
			float r = dx*dx + dy*dy + dz*dz;
			r += es;
			r = 1.0f/sqrt(r);
			r *= r*r;
			float f = m2 * r;

			if (i==j) f = 0.0;
	
			ax += f*dx;
			ay += f*dy;
			az += f*dz;
	
		}

		x += vx * gdt + 0.5f*gdt*gdt*ax;
		y += vy * gdt + 0.5f*gdt*gdt*ay;;
		z += vz * gdt + 0.5f*gdt*gdt*az;;
	
		vx += ax * gdt;
		vy += ay * gdt;
		vz += az * gdt;

		pp[__index_x(i)] = x;
		pp[__index_y(i)] = y;
		pp[__index_z(i)] = z;
	
		vv[__index_x(i)] = vx;
		vv[__index_y(i)] = vy;
		vv[__index_z(i)] = vz;

	}

	}

	Stop(0);
	double time = GetElapsedTime(0); 
	step_count += nburst;

	if (time > 5.0) {	
	gflops = __gflops(time,step_count,nparticle);
		Reset(0);
		step_count = 0;
	}

}

#ifdef ENABLE_CL
void iterate_cl(
   int nburst, int nparticle, int nthread,
   float gdt, float es,
   float* pp, float* vv
)
{
	double time_old = GetElapsedTime(0);

	cl_float* pp2 = (cl_float*)clmalloc(cp,sizeof(cl_float)*4*nparticle,0);
//	for(int i=0;i<4*nparticle;i+=4) pp2[i+3] = pp[i+3];

//	int n = nparticle/4;
	int n = nparticle/2;

	clndrange_t ndr = clndrange_init1d(0,n,nthread);
	clarg_assert_proto(nbody_kern,nparticle,gdt,es,pp,vv,pp,0);
	clarg_set(k_nbody,0,n);
	clarg_set(k_nbody,1,gdt);
	clarg_set(k_nbody,2,es);
	clarg_set_global(k_nbody,4,(cl_float4*)vv);
	clarg_set_local(k_nbody,6,4*nthread*sizeof(cl_float4));

	clmsync(cp,0,pp,CL_MEM_DEVICE|CL_EVENT_RELEASE|CL_EVENT_NOWAIT);
	clmsync(cp,0,vv,CL_MEM_DEVICE|CL_EVENT_RELEASE|CL_EVENT_NOWAIT);
	clwait(cp,0,CL_MEM_EVENT|CL_EVENT_RELEASE);
	Start(0); 

	for(int burst = 0; burst<nburst; burst+=2) {

		clarg_set_global(k_nbody,3,(cl_float4*)pp2);
		clarg_set_global(k_nbody,5,(cl_float4*)pp);
		clfork(cp,0,k_nbody,&ndr,CL_EVENT_NOWAIT);

		clarg_set_global(k_nbody,3,(cl_float4*)pp);
		clarg_set_global(k_nbody,5,(cl_float4*)pp2);
		clfork(cp,0,k_nbody,&ndr,CL_EVENT_NOWAIT);

	}

	clwait(cp,0,CL_KERNEL_EVENT|CL_EVENT_RELEASE);
	Stop(0);

	clmsync(cp,0,pp,CL_MEM_HOST|CL_EVENT_RELEASE|CL_EVENT_NOWAIT);
	clmsync(cp,0,vv,CL_MEM_HOST|CL_EVENT_RELEASE|CL_EVENT_NOWAIT);
	clwait(cp,0,CL_MEM_EVENT|CL_EVENT_RELEASE);

//	Stop(0);

	double time = GetElapsedTime(0);

	step_count += nburst;

	if (time > 2.0) {	
		gflops = __gflops(time,step_count,nparticle);
		Reset(0);
		step_count = 0;
	}

	clfree(pp2);

}

#endif

