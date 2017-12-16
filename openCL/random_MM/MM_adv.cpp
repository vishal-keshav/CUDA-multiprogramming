#include <stdlib.h>
#include <iostream>
#include <time.h>
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

using namespace std;
int N = 1024;
int M = 1024;
int K = 1024;

#define N_size 1024
#define M_size 1024
#define K_size 1024

int data1[M_size*K_size];
int data2[K_size*N_size];
//int data2_trans[N_size*K_size];
int gpu_output[M_size*N_size];
int cpu_output[M_size*N_size];

int err;
cl_platform_id platform_id = NULL;
cl_uint nr_platforms;
cl_device_id device_id = NULL;
cl_uint nr_devices;
cl_context context = NULL;
cl_command_queue command_queue = NULL;
cl_mem memobj_a = NULL;
cl_mem memobj_b = NULL;
cl_mem memobj_c = NULL;

cl_mem memobj_b_trans = NULL;

cl_program prog  = NULL;
cl_program prog_trans = NULL;
cl_kernel kernel_trans = NULL;
cl_kernel kernel = NULL;

bool gpu_init(void){
  char cBuffer[1024];
  err = clGetPlatformIDs(1, &platform_id, &nr_platforms);
  if(err != CL_SUCCESS){
    cout << "Error getting platform " << err << endl;
    return false;
  }
  cout << "Number of platforms " << nr_platforms << endl;

  err = clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, sizeof(cBuffer), cBuffer, NULL);
  if(err != CL_SUCCESS){
    cout << "Error getting platform info " << err <<endl;
    return false;
  }
  cout << "Platform is " << cBuffer << endl;

  err = clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, sizeof(cBuffer), cBuffer, NULL);
  if(err != CL_SUCCESS){
    cout << "Error getting platform version " << err << endl;
    return false;
  }
  cout << "Platform version is " << cBuffer << endl;

  err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &nr_devices);
  if(err != CL_SUCCESS){
    cout << "Error getting device id " << err << endl;
    return false;
  }
  cout << "Number of devices " << nr_devices << endl;

  context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &err);
  if(err != CL_SUCCESS){
    cout << "Error getting context " << err << endl;
    return false;
  }
  cout << "Context created" << endl;

  command_queue = clCreateCommandQueue(context, device_id, 0, &err);
  if(err != CL_SUCCESS){
    cout << "Error creating command queue " << err << endl;
    return false;
  }
  cout << "Command queue created" << endl;
  memobj_a = clCreateBuffer(context, CL_MEM_READ_ONLY , M*K*sizeof(int), NULL, &err);
  memobj_b = clCreateBuffer(context, CL_MEM_READ_ONLY, K*N*sizeof(int), NULL, &err);
  memobj_c = clCreateBuffer(context, CL_MEM_READ_WRITE, M*N*sizeof(int), NULL, &err);
  memobj_b_trans = clCreateBuffer(context, CL_MEM_READ_WRITE, N*K*sizeof(int), NULL, &err);
  if(err != CL_SUCCESS){
    cout << "Error creating device memory buffer " << err << endl;
    return false;
  }
  cout << "Memory allocated" << endl;
  return true;
}

bool setup_program(string name, string trans){

  FILE *fp;
  fp = fopen((trans+".cl").c_str(), "r");
  if(!fp){
    cout << "Failed to load kernel" << endl;
    return false;
  }
  fseek(fp, 0, SEEK_END);
  size_t source_size = ftell(fp);
  rewind(fp);
  char* source_str = (char*)malloc(source_size);
  fread(source_str, 1, source_size, fp);
  fclose(fp);
  cout << "File read success, source size is " << source_size << endl;
  cout << source_str << endl;

  prog_trans = clCreateProgramWithSource(context, 1, (const char **)&source_str, (const size_t *)&source_size, &err);
  if(err != CL_SUCCESS){
    cout << "Unable to create program from source " << err << endl;
    return false;
  }
  else{
    cout << "Program object created" << endl;
  }
  err = clBuildProgram(prog_trans, 1, &device_id, NULL, NULL, NULL);
  if(err != CL_SUCCESS){
    cout << "Unable to compile kernel program " << err << endl;
    char *log = new char[1024];
    err = clGetProgramBuildInfo(prog_trans, device_id, CL_PROGRAM_BUILD_LOG, 1024, log, NULL);
    cout << log << endl;
    return false;
  }
  else{
    cout << "Program building done" << endl;
    size_t bin_size;
    err = clGetProgramInfo(prog_trans, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &bin_size, NULL);
    char *bin = new char[bin_size];
    err = clGetProgramInfo(prog_trans, CL_PROGRAM_BINARIES, sizeof(unsigned char *), &bin, NULL);
    fp = fopen((trans + ".ptx").c_str(), "wb");
    fwrite(bin, sizeof(char), bin_size, fp);
    fclose(fp);
    free(bin);
  }
  kernel_trans = clCreateKernel(prog_trans, trans.c_str(), &err);
  if(err != CL_SUCCESS){
    cout << "Unable to create kernel object " << err << endl;
    return false;
  }
  else{
    cout << "Kernel object created from compiled program" << endl;
  }
  // For main kernel
  fp = fopen((name+".cl").c_str(), "r");
  if(!fp){
    cout << "Failed to load kernel" << endl;
    return false;
  }
  fseek(fp, 0, SEEK_END);
  source_size = ftell(fp);
  rewind(fp);
  char* source_str_main = (char*)malloc(source_size);
  fread(source_str_main, 1, source_size, fp);
  fclose(fp);
  cout << "File read success, source size is " << source_size << endl;
  cout << source_str_main << endl;

  prog = clCreateProgramWithSource(context, 1, (const char **)&source_str_main, (const size_t *)&source_size, &err);
  if(err != CL_SUCCESS){
    cout << "Unable to create program from source " << err << endl;
    return false;
  }
  else{
    cout << "Program object created" << endl;
  }
  err = clBuildProgram(prog, 1, &device_id, NULL, NULL, NULL);
  if(err != CL_SUCCESS){
    cout << "Unable to compile kernel program " << err << endl;
    char *log_main = new char[1024];
    err = clGetProgramBuildInfo(prog, device_id, CL_PROGRAM_BUILD_LOG, 1024, log_main, NULL);
    cout << log_main << endl;
    return false;
  }
  else{
    cout << "Program building done" << endl;
    size_t bin_size;
    err = clGetProgramInfo(prog, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &bin_size, NULL);
    char *bin = new char[bin_size];
    err = clGetProgramInfo(prog, CL_PROGRAM_BINARIES, sizeof(unsigned char *), &bin, NULL);
    fp = fopen((name + ".ptx").c_str(), "wb");
    fwrite(bin, sizeof(char), bin_size, fp);
    fclose(fp);
    free(bin);
  }
  kernel = clCreateKernel(prog, name.c_str(), &err);
  if(err != CL_SUCCESS){
    cout << "Unable to create kernel object " << err << endl;
    return false;
  }
  else{
    cout << "Kernel object created from compiled program" << endl;
  }
  return true;
}

void gpu_deinit(void){
  clReleaseCommandQueue(command_queue);
  clReleaseProgram(prog);
  clReleaseKernel(kernel);
  clReleaseProgram(prog_trans);
  clReleaseKernel(kernel_trans);
  clReleaseMemObject(memobj_a);
  clReleaseMemObject(memobj_b);
  clReleaseMemObject(memobj_c);
  clReleaseMemObject(memobj_b_trans);
  clReleaseContext(context);
}

int main(){
  clock_t temp, cpu_time, gpu_time;

  //Generate input dataset
  srand(3);
  for(int i=0;i<M_size;i++){
    for(int j=0;j<N_size;j++){
      data1[i*N + j] = rand()%100;
    }
  }
  for(int i=0;i<N_size;i++){
    for(int j=0;j<M_size;j++){
      data2[i*M + j] = rand()%100;
    }
  }

  //Performing multiplication on cpu
  temp = clock();
  for(int i=0;i<M_size;i++){
    for(int j=0;j<N_size;j++){
      cpu_output[i*N_size + j] = 0;
      for(int k=0;k<K_size;k++){
        cpu_output[i*K_size + j] += data1[i*K_size + k]*data2[k*N_size + j];
      }
    }
  }
  cpu_time = (float)(clock()-temp);

  //Setting up gpu for computation
  if(!gpu_init()){
    cout << "Error while gpu init" << endl;
    return 0;
  }
  //Setup Program
  if(!setup_program("MM_level6" , "trans_level2")){
    cout << "Could not setup program" << endl;
    return 0;
  }

  err = clSetKernelArg(kernel_trans, 0, sizeof(cl_mem), (void *)&memobj_b);
  err = clSetKernelArg(kernel_trans, 1, sizeof(cl_mem), (void *)&memobj_b_trans);
  err = clSetKernelArg(kernel_trans, 2, sizeof(int), (void *)&M);
  err = clSetKernelArg(kernel_trans, 3, sizeof(int), (void *)&N);

  err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&memobj_a);
  err = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&memobj_b_trans);
  err = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&memobj_c);

  if(err != CL_SUCCESS){
    cout << "Arguments cannot be set " << err << endl;
    return 0;
  }
  else{
    cout << "Kernel arguments set" << endl;
  }

  err = clEnqueueWriteBuffer(command_queue, memobj_a, CL_TRUE, 0, M*K*sizeof(*data1), data1, 0, NULL, NULL);
  err = clEnqueueWriteBuffer(command_queue, memobj_b, CL_TRUE, 0, K*N*sizeof(*data2), data2, 0, NULL, NULL);

  size_t localWorkSize_trans[2] = {32, 32};
  size_t globalWorkSize_trans[2] = {1024, 1024};

  size_t WPT = 8;
  size_t localWorkSize[2] = {128/WPT, 128/WPT};
  size_t globalWorkSize[2] = {1024/WPT, 1024/WPT};
  //Performing transpose on gpu
  temp = clock();
  err = clEnqueueNDRangeKernel(command_queue, kernel_trans, 2, NULL, globalWorkSize_trans, localWorkSize_trans, 0, NULL, NULL);
  if(err != CL_SUCCESS){
    cout << "Task cannot be enqueued " << err << endl;
    return 0;
  }
  else{
    cout << "GPU computation done" << endl;
  }
  err = clFinish(command_queue);
  /*err = clEnqueueReadBuffer(command_queue, memobj_b_trans, CL_TRUE, 0, N*K*sizeof(int), data2_trans, 0, NULL, NULL);
  bool trans_check = true;
  for(int i=0;i<N;i++){
    for(int j=0;j<K;j++){
      if(data2_trans[i*K + j] != data2[j*N + i]){
        trans_check = false;
        break;
      }
    }
    if(!trans_check){
      cout << "Wrongly transpose" << endl;
      break;
    }
  }*/
  err = clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
  if(err != CL_SUCCESS){
    cout << "Task cannot be enqueued " << err << endl;
    return 0;
  }
  else{
    cout << "GPU computation done" << endl;
  }
  err = clFinish(command_queue);
  gpu_time = (float)(clock()-temp);
  //Reading gpu computed Results
  err = clEnqueueReadBuffer(command_queue, memobj_c, CL_TRUE, 0, M*N*sizeof(int), gpu_output, 0, NULL, NULL);

  if(err != CL_SUCCESS){
    cout << "Data cannot be read " << err << endl;
    return 0;
  }
  else{
    cout << "Data read done" << endl;
  }
  gpu_deinit();

  bool check = true;
  for(int i=0;i<M;i++){
    for(int j=0;j<N;j++){
      if(cpu_output[i*N + j]!=gpu_output[i*N + j]){
        check = false;
        break;
      }
    }
    if(!check){
      break;
    }
  }

  if(check){
    cout << "CPU TIME: " << cpu_time << "  GPU TIME: " << gpu_time << endl;
  }
  else{
    for(int i=0;i<M;i++){
      for(int j=0;j<N;j++){
        //cout << "(" <<cpu_output[i*N + j] << "," << gpu_output[i*N + j] << ") ";
        if(cpu_output[i*N + j]!=gpu_output[i*N + j]){
          cout << " ( " << i << "," << j << " ) " << endl;
        }
      }
      cout << endl;
    }
    cout << "Results does not match" << endl;
  }
  return 0;
}
