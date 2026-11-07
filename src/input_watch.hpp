#pragma once
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
// Retain change notifications even when bytes and timestamps are later restored.
class RenderInputWatch {
 juce::Array<juce::File> inputs;
#if JUCE_WINDOWS
 struct Directory {
  juce::File path;HANDLE handle=INVALID_HANDLE_VALUE,event=nullptr;OVERLAPPED pending{};alignas(DWORD) unsigned char buffer[65536]{};bool armed=false;DWORD filter=FILE_NOTIFY_CHANGE_DIR_NAME;
  ~Directory(){if(handle!=INVALID_HANDLE_VALUE){if(armed){CancelIoEx(handle,&pending);DWORD bytes=0;GetOverlappedResult(handle,&pending,&bytes,TRUE);}CloseHandle(handle);}if(event)CloseHandle(event);}
  void arm(){ResetEvent(event);pending={};pending.hEvent=event;require(ReadDirectoryChangesW(handle,buffer,sizeof(buffer),FALSE,filter,nullptr,&pending,nullptr)!=0,"Cannot arm render input watch");armed=true;}
 };
 std::vector<std::unique_ptr<Directory>> directories;
#endif
 juce::String changed,failure;std::mutex mutex;std::condition_variable wake;std::atomic<bool> stopping{false};std::thread thread;uint64_t cycle=0;
 void stop(){stopping=true;wake.notify_all();if(thread.joinable())thread.join();}
public:
 ~RenderInputWatch(){stop();}
 void start(const juce::File& folder){
  stop();changed.clear();failure.clear();cycle=0;inputs.clear();
#if JUCE_WINDOWS
  directories.clear();
#endif
  auto manifest=juce::JSON::parse(folder.getChildFile("inputs.json"));if(!manifest.isArray())return;
  for(auto entry:*manifest.getArray())inputs.addIfNotAlreadyThere(juce::File(entry["path"].toString()));
#if JUCE_WINDOWS
  juce::Array<juce::File> parents;
  for(auto input:inputs)for(auto parent=input.getParentDirectory();;){parents.addIfNotAlreadyThere(parent);auto next=parent.getParentDirectory();if(next==parent)break;parent=next;}
  for(auto path:parents){auto watch=std::make_unique<Directory>();watch->path=path;for(auto input:inputs)if(input.getParentDirectory()==path)watch->filter|=FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_SIZE;watch->handle=CreateFileW(path.getFullPathName().toWideCharPointer(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,nullptr);require(watch->handle!=INVALID_HANDLE_VALUE,"Cannot watch render input directory");watch->event=CreateEventW(nullptr,TRUE,FALSE,nullptr);require(watch->event!=nullptr,"Cannot create render watch event");watch->arm();directories.push_back(std::move(watch));}
#endif
  stopping=false;thread=std::thread([this]{while(!stopping){juce::String error;try{drain();}catch(const std::exception& e){error=juce::String::fromUTF8(e.what());}{std::lock_guard<std::mutex> lock(mutex);if(error.isNotEmpty())failure=error;++cycle;}wake.notify_all();if(error.isNotEmpty())break;std::unique_lock<std::mutex> lock(mutex);wake.wait_for(lock,std::chrono::milliseconds(5));}});
 }
 void validate(){if(!thread.joinable())return;std::unique_lock<std::mutex> lock(mutex);auto next=cycle+1;wake.notify_all();wake.wait(lock,[&]{return cycle>=next||failure.isNotEmpty()||stopping.load();});if(failure.isNotEmpty())throw std::runtime_error(failure.toStdString());}
private:
 void drain(){
#if JUCE_WINDOWS
  for(auto& watch:directories)for(int batch=0;batch<64;++batch){auto status=WaitForSingleObject(watch->event,0);if(status==WAIT_TIMEOUT)break;require(status==WAIT_OBJECT_0,"Render watch failed");DWORD bytes=0;require(GetOverlappedResult(watch->handle,&watch->pending,&bytes,FALSE)!=0,"Render input watch lost events");watch->armed=false;if(bytes==0)throw std::runtime_error(("Render input watch overflow; refusing delivery: "+watch->path.getFullPathName()).toStdString());
   size_t offset=0;for(;;){require(offset+offsetof(FILE_NOTIFY_INFORMATION,FileName)<=bytes,"Invalid render change event");auto* entry=reinterpret_cast<FILE_NOTIFY_INFORMATION*>(watch->buffer+offset);require(offset+offsetof(FILE_NOTIFY_INFORMATION,FileName)+entry->FileNameLength<=bytes,"Truncated render change event");auto file=watch->path.getChildFile(juce::String(entry->FileName,int(entry->FileNameLength/sizeof(WCHAR))));for(auto input:inputs)if(file==input||(entry->Action!=FILE_ACTION_MODIFIED&&input.isAChildOf(file)))changed=file.getFullPathName();if(entry->NextEntryOffset==0)break;require(entry->NextEntryOffset>0&&offset+entry->NextEntryOffset<bytes,"Invalid render event offset");offset+=entry->NextEntryOffset;}
   watch->arm();require(batch<63,"Render change queue remained busy; refusing unverified delivery");if(changed.isNotEmpty())break;
  }
#endif
  if(changed.isNotEmpty())throw std::runtime_error(("Render input changed during task: "+changed).toStdString());
 }
};
