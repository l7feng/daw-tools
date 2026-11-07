#pragma once
static void copyRenderStaging(const juce::File& source,const juce::File& target,std::function<void(int64_t,int64_t)> progress={}){
 require(source.existsAsFile()&&!target.exists()&&target.getParentDirectory().isDirectory(),"Invalid render staging copy");
#if JUCE_WINDOWS
 struct Context {
  std::function<void(int64_t,int64_t)> progress;std::exception_ptr error;
  static DWORD CALLBACK update(LARGE_INTEGER total,LARGE_INTEGER copied,LARGE_INTEGER,LARGE_INTEGER,DWORD,DWORD,HANDLE,HANDLE,LPVOID opaque){auto& c=*static_cast<Context*>(opaque);try{if(c.progress)c.progress(copied.QuadPart,total.QuadPart);return PROGRESS_CONTINUE;}catch(...){c.error=std::current_exception();return PROGRESS_CANCEL;}}
 } context{std::move(progress),{}};
 auto ok=CopyFileExW(source.getFullPathName().toWideCharPointer(),target.getFullPathName().toWideCharPointer(),Context::update,&context,nullptr,COPY_FILE_FAIL_IF_EXISTS);if(context.error)std::rethrow_exception(context.error);require(ok!=0,"Render staging copy failed; no delivery committed");
#else
 std::filesystem::copy_file(std::filesystem::path(source.getFullPathName().toStdString()),std::filesystem::path(target.getFullPathName().toStdString()));if(progress)progress(source.getSize(),source.getSize());
#endif
 require(juce::SHA256(source).toHexString()==juce::SHA256(target).toHexString(),"Render staging copy hash differs");
}
