#!/home/malo/bin/instantcpp --std=c++20 -Ofast -Wall -Wextra -pedantic -I/data/Documents/src/lib/cpp/.

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <SError.h>
#include <srcinfo.h>
#include <thread>
#include <cstring>
#include <mutex>
#include <string_view>

namespace std_fs = std::filesystem;

constexpr std::string_view htlm_header{
"<!DOCTYPE html>\n\
<HTML>\n\
        <header>\n\
            <script type=\"text/javascript\">\n\
            function mouseOver(e) {\n\
                var BBL =  document.getElementById('Bubble');\n\
                BBL.style.visibility = 'visible';\n\
                BBL.innerHTML = \"exif : \"+event.currentTarget.alt;\n\
            }\n\
\n\
            function mouseOut(e) {\n\
                var BBL =  document.getElementById('Bubble');\n\
                BBL.style.visibility = 'hidden';\n\
            }\n\
\n\
            function main(){\n\
                var elms = document.querySelectorAll(\"[id='myimage']\");\n\
                for(var i = 0; i < elms.length; i++){\n\
                 elms[i].addEventListener('mouseover', mouseOver);\n\
                 elms[i].addEventListener('mouseout', mouseOut);\n\
                }\n\
            }\n\
                </script>\n\
        </header>\n\
<BODY onload=\"main()\">\n\
<H1> Index of video preview</H1>\n\
<pre><div id='Bubble' style=\"position: fixed; right: 40%; top: 100px; width:250px; height: 200px; visibility: hidden;\"></div></pre>\n"
};

std::string html_line(std::string orgpath,std::string gifname,size_t cnt){
    std::stringstream ss;
    ss << "<a href=\"" << orgpath << "\"> <img id='myimage' src=\"" << gifname << "\" alt=\"NO EXIF Data Availabe\" width=\"100\" height=\"100\"></a>\n";
    if( (cnt%10) == 0 ) ss << "\n<br>\n";
    return(ss.str());
}

constexpr std::string_view html_end{ "</BODY>\n</HTML>" };

ssize_t doublefile_count(std::string& fname){
    size_t cnt=1; // muss mit 1 starten !

    if( ! std_fs::exists(fname) ) return(0);
    std::string newname=std::to_string(cnt)+"_"+fname;
    while(1){
        if( ! std_fs::exists(newname) ) break;
        ++cnt;
        if( cnt > 100 ){
            cnt=-1;
            break;
        }
        newname=std::to_string(cnt)+"_"+fname;
    } // end of unconditioned  while-llop

    return(cnt);
}

bool RunJobs(std::vector<std::string>& cmds,size_t& total_files_converted){
    bool okay=true;
    try{
        if( cmds.size() < 1 ) throw SError() << __STDINF__ << "INFO: all jobs done.";
        // cmds reached enough entries to work multi threaded : converting videos:
        std::cout << __STDINF__ << time(0L) << " INFO: start converting " << cmds.size() << " videos\n";
        std::vector<std::thread> t;
        std::mutex g_num_mutex;
        for(auto m : cmds){
            t.push_back(std::thread([m,&g_num_mutex,&total_files_converted]() 
            {
                int rc=system(m.c_str());
                if( rc != 0 ){
                    std::cerr << __STDINF__ << "ERROR: in thread" << std::this_thread::get_id() << " : command failed: " << m << "\n";
                }else{
                    g_num_mutex.lock();
                    ++total_files_converted;
                    g_num_mutex.unlock();
                }
            }));
        }
        
        for(auto& m : t){
            m.join();
        }

        std::cout << __STDINF__ << time(0L) << " INFO: converted " << cmds.size() << " videos\n";
        cmds.clear();
    }catch(const std::exception& e){
        std::cerr <<  e.what() << std::endl;
    }
    return(okay);
}

int main(int argc,char **argv){
    int rc=0;
    
    try{
        std::vector<std::string> allArgs(argv,argv+argc);

        if( allArgs.size() != 2 ) throw SError() << __STDINF__ << "ERROR: need a path name as argument";
        std::string path=allArgs.at(1);

        std::ofstream htmlout("./video_pic_catalog.html");
        if( ! htmlout.is_open() ) throw SError() << __STDINF__ << "ERROR: unable to open html catalog output file: " << strerror(errno);

        htmlout << htlm_header << "\n";
        htmlout << "video preview catalog from source path: " << path << "<br>\n";

        size_t max_threads=(std::thread::hardware_concurrency());
        if( max_threads > 1 ) max_threads=max_threads/2;
        std::vector<std::string> cmds;
        std::cout << __STDINF__ << "INFO: using thread amount of " << max_threads << std::endl;
        size_t total_files_found=0;
        size_t total_files_converted=0;
        for(auto& p: std_fs::recursive_directory_iterator(path)){
            if( ! std_fs::is_regular_file(p) ) continue;
            if( p.path().extension() != ".mp4" &&  p.path().extension() != ".MP4" ) continue;

            ++total_files_found;
            std::stringstream ss;
            std::string outputfile="prev_";
            outputfile+=p.path().filename().replace_extension(".gif").c_str();
            ssize_t fcount=doublefile_count(outputfile); // wenn hier als 0 raus kommt existiert das preview gif bereits und wir nehmen fcount
            if( fcount < 0 ){
                // wir haben hier einen Fehler ! z.B. wenn mehr als 100 file mit dem selben namen bereits existieren !
                std::cerr << __STDINF__ << "WARNING: doublefile_count() raised an error ! skiping conver of: " << p.path().c_str() << std::endl;
                continue; // naechstes file versuchen !
            }
            if( fcount >  0 ) outputfile=std::to_string(fcount)+"_"+outputfile;
            ss << "ffmpeg -hide_banner -loglevel error -ss 0 -t 3 -i \"" << p.path().c_str() << "\" -vf \"fps=10,scale=320:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse\" -loop 0 \"" << outputfile << "\" 1>convert.log 2>&1";
            cmds.push_back(ss.str());
            htmlout << html_line(p.path().c_str(),outputfile,total_files_found);
            if( cmds.size() < max_threads) continue;
            RunJobs(cmds,total_files_converted);
        } // end of for-loop

        RunJobs(cmds,total_files_converted);

        htmlout << "<hr>\n";
        htmlout << "reached end of catalog. done @" << time(0L) << " : found " << total_files_found << " : converted files: " << total_files_converted << " file<br>\n";
        htmlout << html_end << "\n";
        htmlout.close();
        std::cout << "vide_preview.cpp // reached end of catalog. done @" << time(0L) << " : found " << total_files_found << " : converted files: " << total_files_converted << " file\n";
    }catch(const std::exception& e){
        std::cerr << e.what() << std::endl;
    }

    return(rc);
}

