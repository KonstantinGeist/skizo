// *****************************************************************************
//
//  Copyright (c) Konstantin Geist. All rights reserved.
//
//  The use and distribution terms for this software are contained in the file
//  named License.txt, which can be found in the root of this distribution.
//  By using this software in any fashion, you are agreeing to be bound by the
//  terms of this license.
//
//  You must not remove this notice, or any other, from this software.
//
// *****************************************************************************

#include "../icall.h"
#include "../Domain.h"
#include "../HashMap.h"
#include "../RuntimeHelpers.h"

namespace skizo { namespace script {
using namespace skizo::collections;

extern "C" {

void* SKIZO_API _so_Map_createImpl()
{
    return new SkizoMapObject();
}

void SKIZO_API _so_Map_destroyImpl(void* ptr)
{
    if(ptr) {
        delete (SkizoMapObject*)ptr;
    }
}

static void getMapKey(void* _mapObj, void* key, SkizoMapObjectKey* mapKey)
{
    const SkizoMapObject* mapObj = (SkizoMapObject*)_mapObj;
    const CClass* keyClass = so_class_of(key);

    mapKey->Key = key;

    if(mapObj->KeyClassCache != keyClass) {
        FHashCode hashCodeMethodPtr;
        FEquals equalsMethodPtr;

        keyClass->GetMapMethods(key, &hashCodeMethodPtr, &equalsMethodPtr);
        if(!hashCodeMethodPtr || !equalsMethodPtr) {
            CDomain::Abort("Passed key doesn't implement '(hashCode):int' or '(equals obj: any): bool'.");
        }

        mapObj->KeyClassCache = keyClass;
        mapObj->HashCodeMethodPtr = hashCodeMethodPtr;
        mapObj->EqualsMethodPtr = equalsMethodPtr;
    }

    mapKey->HashCode = mapObj->HashCodeMethodPtr(key);
    mapKey->EqualsMethodPtr = mapObj->EqualsMethodPtr;
}

void* SKIZO_API _so_Map_getImpl(void* _mapObj, void* key)
{
    if(!key) {
        CDomain::Abort("Key can't be null.");
    }

    const SkizoMapObject* mapObj = (SkizoMapObject*)_mapObj;

    SkizoMapObjectKey mapKey;
    getMapKey(_mapObj, key, &mapKey);

    void* r = nullptr;
    if(!mapObj->BackingMap->TryGet(mapKey, &r)) {
        CDomain::Abort("Key not found.");
    }

    return r;
}

_so_bool SKIZO_API _so_Map_containsImpl(void* _mapObj, void* key)
{
    if(!key) {
        CDomain::Abort("Key can't be null.");
    }

    const SkizoMapObject* mapObj = (SkizoMapObject*)_mapObj;
    SkizoMapObjectKey mapKey;
    getMapKey(_mapObj, key, &mapKey);

    return mapObj->BackingMap->Contains(mapKey);
}

void SKIZO_API _so_Map_setImpl(void* _mapObj, void* key, void* value)
{
    if(!key) {
        CDomain::Abort("Key can't be null.");
    }

    SkizoMapObject* mapObj = (SkizoMapObject*)_mapObj;

    SkizoMapObjectKey mapKey;
    getMapKey(_mapObj, key, &mapKey);

    mapObj->BackingMap->Set(mapKey, value);
}

void SKIZO_API _so_Map_removeImpl(void* _mapObj, void* key)
{
    if(!key) {
        CDomain::Abort("Key can't be null.");
    }

    SkizoMapObject* mapObj = (SkizoMapObject*)_mapObj;

    SkizoMapObjectKey mapKey;
    getMapKey(_mapObj, key, &mapKey);

    mapObj->BackingMap->Remove(mapKey);
}

void SKIZO_API _so_Map_clearImpl(void* _mapObj)
{
    SkizoMapObject* mapObj = (SkizoMapObject*)_mapObj;
    mapObj->BackingMap->Clear();
}

int SKIZO_API _so_Map_sizeImpl(void* _mapObj)
{
    const SkizoMapObject* mapObj = (SkizoMapObject*)_mapObj;
    return mapObj->BackingMap->Size();
}

void SKIZO_API _so_Map_loopImpl(void* _mapObj, void* mapLooper)
{
    SKIZO_NULL_CHECK(mapLooper);

    typedef _so_bool (SKIZO_API * _FMapLooper)(void*, void* key, void* value);
    _FMapLooper mapLoopFunc = (_FMapLooper)so_invokemethod_of(mapLooper);
    const SkizoMapObject* mapObj = (SkizoMapObject*)_mapObj;

    SHashMapEnumerator<SkizoMapObjectKey, void*> mapEnum (mapObj->BackingMap);
    SkizoMapObjectKey key;
    void* value;
    while(mapEnum.MoveNext(&key, &value)) {
        if(!mapLoopFunc(mapLooper, key.Key, value)) {
            break;
        }
    }
}

}

} }
