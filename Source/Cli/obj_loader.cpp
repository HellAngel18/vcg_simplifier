#include "obj_loader.h"

// VCG Headers
#include <vcg/complex/complex.h>
#include <wrap/io_trimesh/export.h>
#include <wrap/io_trimesh/import.h>

bool LoadObj(MyMesh &m, const std::string &filename) {
    int loadMask = vcg::tri::io::Mask::IOM_WEDGTEXCOORD;
    int err      = vcg::tri::io::Importer<MyMesh>::Open(m, filename.c_str(), loadMask);

    if (err) {
        printf("Error: %s\n", vcg::tri::io::Importer<MyMesh>::ErrorMsg(err));
        if (vcg::tri::io::Importer<MyMesh>::ErrorCritical(err))
            return 0;
    }
    return 1;
}

bool SaveObj(MyMesh &m, const std::string &filename) {
    vcg::tri::io::Exporter<MyMesh>::Save(m, filename.c_str(), vcg::tri::io::Mask::IOM_WEDGTEXCOORD);
    return 1;
}