#ifndef POI_H
#define POI_H

// Windows headers required for eckernel.h
#ifdef _WIN32
#include <windows.h>
#include <wingdi.h>
#endif

#include <QDateTime>
#include <QString>
#include <limits>

#include <eckernel.h>

#ifndef EC_POI_FLAG_ACTIVE
#define EC_POI_FLAG_ACTIVE        (1U << 0)
#define EC_POI_FLAG_PERSISTENT    (1U << 1)
#define EC_POI_FLAG_USER_DEFINED  (1U << 2)
#endif

#ifndef EC_POI_CATEGORY_DEFINED
#define EC_POI_CATEGORY_DEFINED
typedef enum _EcPoiCategory
{
    EC_POI_GENERIC = 0,
    EC_POI_CHECKPOINT,
    EC_POI_HAZARD,
    EC_POI_SURVEY_TARGET
} EcPoiCategory;
#endif

#ifndef EC_POI_DEFINITION_STRUCT_DEFINED
#define EC_POI_DEFINITION_STRUCT_DEFINED
struct EcPoiDefinitionStruct
{
    EcCoordinate    latitude;
    EcCoordinate    longitude;
    EcCoordinate    depth;
    const char     *label;
    const char     *description;
    EcPoiCategory   category;
    UINT32          flags;
};
#endif

#ifndef EC_POI_HANDLE_STRUCT_DEFINED
#define EC_POI_HANDLE_STRUCT_DEFINED
struct EcPoiHandleStruct;
#endif

#ifndef EC_POI_TYPEDEF_DEFINED
#define EC_POI_TYPEDEF_DEFINED
typedef struct EcPoiHandleStruct *EcPoiHandle;
typedef Bool (*EcPoiVisitProc)(EcPoiHandle poi, const EcPoiDefinitionStruct *definition, void *userData);
#endif

class Poi
{
public:
    using Handle = EcPoiHandle;
    using Definition = EcPoiDefinitionStruct;
    using VisitProc = EcPoiVisitProc;

    enum Flag : UINT32
    {
        Active = EC_POI_FLAG_ACTIVE,
        Persistent = EC_POI_FLAG_PERSISTENT,
        UserDefined = EC_POI_FLAG_USER_DEFINED,
    };

    enum Category
    {
        Generic = EC_POI_GENERIC,
        Checkpoint = EC_POI_CHECKPOINT,
        Hazard = EC_POI_HAZARD,
        SurveyTarget = EC_POI_SURVEY_TARGET
    };

    static bool isFlagSet(UINT32 flags, Flag flag)
    {
        return (flags & flag) != 0U;
    }

    static void setFlag(UINT32& flags, Flag flag, bool enabled)
    {
        if (enabled) {
            flags |= flag;
        } else {
            flags &= ~flag;
        }
    }

    // POI Kernel Functions (previously in eckernel.h)
    // These are wrappers for the actual kernel POI functions
    static Handle Create(EcView *view, const Definition *definition)
    {
        // Note: This would call the actual kernel function when available
        // For now, this is a placeholder
        return nullptr;
    }

    static Bool Update(EcView *view, Handle poiHandle, const Definition *definition)
    {
        // Note: This would call the actual kernel function when available
        // For now, this is a placeholder
        return false;
    }

    static Bool Remove(EcView *view, Handle poiHandle)
    {
        // Note: This would call the actual kernel function when available
        // For now, this is a placeholder
        return false;
    }

    static void Enumerate(EcView *view, VisitProc visitProc, void *userData)
    {
        // Note: This would call the actual kernel function when available
        // For now, this is a placeholder
    }
};

struct PoiEntry
{
    int id = -1;
    QString label;
    QString description;
    EcPoiCategory category = EC_POI_GENERIC;
    double latitude = std::numeric_limits<double>::quiet_NaN();
    double longitude = std::numeric_limits<double>::quiet_NaN();
    double depth = std::numeric_limits<double>::quiet_NaN();
    UINT32 flags = EC_POI_FLAG_ACTIVE | EC_POI_FLAG_PERSISTENT;
    EcPoiHandle handle = nullptr;
    QDateTime createdAt = QDateTime::currentDateTimeUtc();
    QDateTime updatedAt = createdAt;
    bool showLabel = true;
};

#endif // POI_H
