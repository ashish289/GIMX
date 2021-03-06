/*
 Copyright (c) 2017 Mathieu Laurendeau <mat.lau@laposte.net>
 License: GPLv3
 */

#include "configupdater.h"
#include <sstream>
#include <string.h>

#include <gimxfile/include/gfile.hpp>

#ifdef WIN32
const char * configupdater::configs_url = "https://api.github.com/repos/matlo/GIMX-configurations/contents/Windows";
const char * configupdater::configs_download_url = "https://raw.githubusercontent.com/matlo/GIMX-configurations/master/Windows/";
#else
const char * configupdater::configs_url = "https://api.github.com/repos/matlo/GIMX-configurations/contents/Linux";
const char * configupdater::configs_download_url =
        "https://raw.githubusercontent.com/matlo/GIMX-configurations/master/Linux/";
#endif

configupdater::configupdater() :
        client_callback(NULL), client_data(NULL) {
}

configupdater::~configupdater() {
}

int configupdater::progress(configupdater::ConfigUpdaterStatus status, double dlnow, double dltotal) {

    return client_callback(client_data, status, dlnow, dltotal);
}

configupdater::ConfigUpdaterStatus convertDowloadStatus(Downloader::DownloaderStatus status) {
    switch (status) {
    case Downloader::DownloaderStatusOk:
        return configupdater::ConfigUpdaterStatusOk;
    case Downloader::DownloaderStatusConnectionPending:
        return configupdater::ConfigUpdaterStatusConnectionPending;
    case Downloader::DownloaderStatusDownloadInProgress:
        return configupdater::ConfigUpdaterStatusDownloadInProgress;
    case Downloader::DownloaderStatusCancelled:
        return configupdater::ConfigUpdaterStatusCancelled;
    case Downloader::DownloaderStatusInitFailed:
        return configupdater::ConfigUpdaterStatusInitFailed;
    case Downloader::DownloaderStatusDownloadFailed:
        return configupdater::ConfigUpdaterStatusDownloadFailed;
    }
    return configupdater::ConfigUpdaterStatusOk;
}

int progressCallback(void *clientp, Downloader::DownloaderStatus status, double progress, double total) {

    configupdater * updater = static_cast<configupdater *>(clientp);

    return updater->progress(convertDowloadStatus(status), progress, total);
}

configupdater::ConfigUpdaterStatus configupdater::getconfiglist(std::list<std::string>& cl, ProgressCallback callback, void * data) {

    client_callback = callback;
    client_data = data;

    std::string tempFile = Downloader::generateTempFile("configs.json");

    Downloader::DownloaderStatus downloadStatus = Downloader().download(configs_url, tempFile, progressCallback, this);

    if (downloadStatus != Downloader::DownloaderStatusOk) {
        return convertDowloadStatus(downloadStatus);
    }

    {
        IFSTREAM(tempFile, infile)

        while (infile.good()) {
            std::string line;
            std::getline(infile, line);
            size_t pos1 = line.find("\"name\": ");
            if (pos1 != std::string::npos) {
                size_t pos2 = line.find("\"", pos1 + strlen("\"name\": "));
                if (pos2 != std::string::npos) {
                    size_t pos3 = line.find(".xml\",", pos2 + 1);
                    if (pos3 != std::string::npos) {
                        cl.push_back(line.substr(pos2 + 1, pos3 + 4 - (pos2 + 1)));
                    }
                }
            }
        }
    } // tempFile is closed at the end of this block, allowing removal

    gfile_remove(tempFile.c_str());

    return configupdater::ConfigUpdaterStatusOk;
}

configupdater::ConfigUpdaterStatus configupdater::getconfig(const std::string& directory, const std::string& config, ProgressCallback callback, void * data) {

    if (directory.empty()) {
        return configupdater::ConfigUpdaterStatusInitFailed;
    }

    client_callback = callback;
    client_data = data;

    std::string url = configs_download_url + config;
    std::string file = directory + config;

    Downloader::DownloaderStatus downloadStatus = Downloader().download(url, file, progressCallback, this);

    if (downloadStatus != Downloader::DownloaderStatusOk) {
        return convertDowloadStatus(downloadStatus);
    }

#ifndef WIN32
    if (gfile_makeown(file.c_str()) < 0) {
        return configupdater::ConfigUpdaterStatusDownloadFailed;
    }
#endif

    return configupdater::ConfigUpdaterStatusOk;
}
