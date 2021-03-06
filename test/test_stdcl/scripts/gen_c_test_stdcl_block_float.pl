#!/usr/bin/perl
# 
# Copyright (c) 2009-2010 Brown Deer Technology, LLC.  All Rights Reserved.
#
# This software was developed by Brown Deer Technology, LLC.
# For more information contact info@browndeertechnology.com
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 3 (LGPLv3)
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# DAR #

$size = 128;
$bsize = 4;
$clfile = 'test_block_float.cl';
$testprefix = 'test_block_';

printf "\n";
printf "#include <stdio.h>\n";
printf "#include <stdlib.h>\n";
printf "#include <math.h>\n";
printf "#include <fcntl.h>\n";
printf "#include <sys/stat.h>\n";
printf "#include <sys/mman.h>\n";
printf "\n";
printf "#include \"stdcl.h\"\n";
printf "\n";
printf "#define SIZE $size\n";
printf "#define BLOCKSIZE $bsize\n";
printf "\n";


printf "#define __mapfile(file,filesz,pfile) do { \\\n";
printf "int fd = open(file,O_RDONLY); \\\n";
printf "struct stat fst; fstat(fd,&fst); \\\n";
printf "if (fst.st_size == 0 || !S_ISREG(fst.st_mode)) { \\\n";
printf "fprintf(stderr,\"error: open failed on '%s'\\n\",file); \\\n";
printf "pfile=0; \\\n";
printf "} else { \\\n";
printf "filesz = fst.st_size; \\\n";
printf "pfile = mmap(0,filesz,PROT_READ,MAP_PRIVATE,fd,0); \\\n";
printf "} \\\n";
printf "close(fd); \\\n";
printf "} while(0);\n";
printf "\n";

printf "#define __exit(err,line) \\\n";
printf "do { fprintf(\"error code %d\\n\",err); exit(line); } while(0)\n";
printf "\n";


printf "int main( int argc, char** argv )\n";
printf "{\n";
printf "int i,j;\n";
printf "cl_uint err; \n";
printf "\n";

printf "CONTEXT* cp = 0;\n";
printf "unsigned int devnum = 0;\n";

printf "size_t size = SIZE;\n";
printf "size_t blocksize = BLOCKSIZE;\n";
printf "\n";
printf "i=1;\n";
printf "char* arg;\n";
printf "while(i<argc) {\n";
printf "arg=argv[i++];\n";
printf "if (!strncmp(arg,\"--size\",6)) size = atoi(argv[i++]);\n";
printf "else if (!strncmp(arg,\"--blocksize\",11)) blocksize = atoi(argv[i++]);\n";
printf "else if (!strncmp(arg,\"--dev\",6)) { cp = stddev; devnum = atoi(argv[i++]); }\n";
printf "else if (!strncmp(arg,\"--cpu\",6)) { cp = stdcpu; devnum = atoi(argv[i++]); }\n";
printf "else if (!strncmp(arg,\"--gpu\",6)) { cp = stdgpu; devnum = atoi(argv[i++]); }\n";
printf "else {\n";
printf "fprintf(stderr,\"unrecognized option '%%s'\\n\",arg);\n";
printf "exit(-1);\n";
printf "}\n";
printf "}\n";
printf "\n";

printf "if (!cp) exit(__LINE__);\n";
printf "if (devnum >= clgetndev(cp)) exit(__LINE__);\n";

for($c=0;$c<10-2;++$c) {
printf "float* aa$c = (float*)clmalloc(cp,size*sizeof(float),0);\n";
printf "if (!aa$c) exit(__LINE__);\n";
printf "float* bb$c = (float*)clmalloc(cp,size*sizeof(float),0);\n";
printf "if (!bb$c) exit(__LINE__);\n";
}


printf "for(i=0;i<size;i++) { \n";
for($c=0;$c<10-2;++$c) {
#printf "aa".$c."[i] = i*1.1f+13.1f*".$c."; bb".$c."[i] = 0; \n";
#printf "aa".$c."[i] = i+13*".$c."; bb".$c."[i] = 0; \n";
printf "aa".$c."[i] = i*1.1f+13.1f*".$c."; bb".$c."[i] = 0; \n";
}
printf "}\n";


printf "void* clh = clopen(cp,\"$clfile\",CLLD_NOW);\n";
printf "cl_kernel krn;\n";

printf "clndrange_t ndr = clndrange_init1d(0,size,blocksize);\n";
printf "cl_event ev[10];\n";
printf "float sum,sum_correct;\n";
#printf "int sum,sum_correct;\n";
printf "float tol = pow(10.0,-8+log10((float)size));\n";

for($c=0;$c<10;++$c) {
   for($a=1;$a<$c;++$a) {
      $b=$c-$a;

printf "krn = clsym(cp,clh,\"$testprefix".$a."_".$b."_kern\",CLLD_NOW);\n";
printf "if (!krn) exit(__LINE__);\n";

for($i=0;$i<$a;++$i) {
printf "if (!clmsync(cp,0,aa$i,CL_MEM_DEVICE|CL_EVENT_NOWAIT))\n";
printf "exit(__LINE__);\n";
}

for($i=0;$i<$a;++$i) {
printf "clarg_set_global(cp,krn,$i,aa$i);\n";
}

for($j=0;$j<$b;++$j) {
$k = $a+$j;
printf "clarg_set_global(cp,krn,$k,bb$j);\n";
}

for($i=0;$i<$a;++$i) {
#printf "if (clSetKernelArg(krn,$i+$a+$b,blocksize*sizeof(int),0)) exit(__LINE__);\n";
printf "clarg_set_local(cp,krn,$i+$a+$b,blocksize*sizeof(float));\n";
}

printf "if (!clfork(cp,0,krn,&ndr,CL_EVENT_NOWAIT))\n";
printf "exit(__LINE__);\n";

for($j=0;$j<$b;++$j) {
printf "if (!clmsync(cp,0,bb$j,CL_MEM_HOST|CL_EVENT_NOWAIT))\n";
printf "exit(__LINE__);\n";
}

printf "clwait(cp,0,CL_KERNEL_EVENT|CL_MEM_EVENT);\n";

printf "sum_correct = 0;\n";
printf "for(i=0;i<$a;++i)\n";
printf "for(j=0;j<$b;++j)\n";
#printf "sum_correct += (j+1.1)*( (size*(size-1)*1.1)/2 + (1+13.1)*i*size + 0.1*size + size*0.4575);\n";
#printf "sum_correct += (j+1.1)*( (size*(size-1)*1.1)/2 + (1+13.1)*i*size + 0.1*size);\n";
#printf "sum_correct += (j+1)*( size*(size-1)/2 + 14*i*size );\n";
#printf "sum_correct += (blocksize-5)*(j+1)*( size*(size-1)/2 + 14*i*size );\n";
printf "sum_correct += (blocksize-5.0)*(j+1.1)*( (size*(size-1)*1.1)/2 + (1+13.1)*i*size + 0.1*size);\n";

printf "sum = 0;\n";
for($j=0;$j<$b;++$j) {
printf "for(i=0;i<size;i++) sum += bb".$j."[i];\n";
}
#printf "printf(\"(%%e) sum ".$a." ".$b." %%e\",sum_correct,sum);\n";
#printf "printf(\" relerr %%e (%%e)\\n\",fabs((sum-sum_correct)/sum_correct),tol);\n";
#printf "if (fabs((sum-sum_correct)/sum_correct) > tol) exit(__LINE__);\n";
#printf "printf(\"(%%d) sum ".$a." ".$b." %%d\\n\",sum_correct,sum);\n";
#printf "if (sum != sum_correct) exit(__LINE__);\n";
printf "printf(\"(%%e) sum ".$a." ".$b." %%e\",sum_correct,sum);\n";
printf "printf(\" relerr %%e (%%e)\\n\",fabs((sum-sum_correct)/sum_correct),tol);\n";
printf "if (fabs((sum-sum_correct)/sum_correct) > tol) exit(__LINE__);\n";


}

}

printf "printf(\"done!\\n\");\n";

printf "clclose(cp,clh);\n";
printf "return(0);\n";
printf "}\n";

