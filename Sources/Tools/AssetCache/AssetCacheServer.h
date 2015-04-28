/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/


#ifndef __DAVAENGINE_ASSET_CACHE_SERVER_H__
#define __DAVAENGINE_ASSET_CACHE_SERVER_H__

#include "Base/BaseTypes.h"
#include "Base/Data.h"

#include "AssetCache/TCPConnection/TCPConnection.h"

namespace DAVA
{

class TCPServer;
namespace AssetCache
{
 
class CachedFiles;
class ClientCacheEntry;

    
class ServerDelegate
{
public:
    
    virtual void OnAddedToCache(const ClientCacheEntry &entry, const CachedFiles &files) = 0;
    virtual void OnIsInCache(const ClientCacheEntry &entry) = 0;
    virtual void OnRequestedFromCache(const ClientCacheEntry &entry) = 0;
};
    
    
class Server: public TCPConnectionDelegate
{
public:
    
    Server();
    virtual ~Server();
    
    void SetDelegate(ServerDelegate * delegate);

    bool Listen(uint16 port);
    
    bool IsConnected() const;
    void Disconnect();
    
    
    //TCPConnectionDelegate
    void ChannelOpen() override;
    void ChannelClosed(const char8* message) override;
    void PacketReceived(const void* packet, size_t length) override;
    void PacketSent() override;
    void PacketDelivered() override;
    //END of TCPConnectionDelegate
    
    bool FilesAddedToCache(const ClientCacheEntry &entry, bool added);
    bool FilesInCache(const ClientCacheEntry &entry, bool isInCache);
    bool SendFiles(const ClientCacheEntry &entry, const CachedFiles &files);
    
private:
    
    bool SendArchieve(KeyedArchive * archieve);

    void OnAddToCache(KeyedArchive * archieve);
    void OnIsInCache(KeyedArchive * archieve);
    void OnGetFromCache(KeyedArchive * archieve);

    
    
private:
    
    TCPServer * netServer = nullptr;
    ServerDelegate *delegate = nullptr;
};

inline void Server::SetDelegate(ServerDelegate * _delegate)
{
    delegate = _delegate;
}
    
    
    
};

};

#endif // __DAVAENGINE_ASSET_CACHE_SERVER_H__

