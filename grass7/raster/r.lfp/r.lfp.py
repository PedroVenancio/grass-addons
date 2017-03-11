#!/usr/bin/env python
############################################################################
#
# MODULE:       r.lfp
# AUTHOR(S):    Huidae Cho
# PURPOSE:      Calculates the longest flow path for a given outlet point.
#
# COPYRIGHT:    (C) 2014, 2017 by the GRASS Development Team
#
#               This program is free software under the GNU General Public
#               License (>=v2). Read the file COPYING that comes with GRASS
#               for details.
#
#############################################################################

#%module
#% description: Calculates the longest flow path for a given outlet point.
#% keyword: hydrology
#% keyword: watershed
#%end
#%option G_OPT_R_INPUT
#% description: Name of input drainage direction raster map
#%end
#%option G_OPT_R_OUTPUT
#% description: Name for output longest flow path raster map
#%end
#%option G_OPT_M_COORDS
#% description: Coordinates of outlet point
#% required: yes
#%end

import sys
import os
import grass.script as grass
from grass.exceptions import CalledModuleError


# check requirements
def check_progs():
    found_missing = False
    prog = 'r.stream.distance'
    if not grass.find_program(prog, '--help'):
        found_missing = True
        grass.warning(_("'%s' required. Please install '%s' first using 'g.extension %s'") % (prog, prog, prog))
    if found_missing:
        grass.fatal(_("An ERROR occurred running r.lfp"))

def main():
    # check dependencies
    check_progs()

    input = options["input"]
    output = options["output"]
    coords = options["coordinates"]

    calculate_lfp(input, output, coords)

def calculate_lfp(input, output, coords):
    prefix = "r_lfp_%d_" % os.getpid()

    # create the outlet vector map
    outlet = prefix + "outlet"
    p = grass.feed_command("v.in.ascii", overwrite=True,
            input="-", output=outlet, separator=",")
    p.stdin.write(coords)
    p.stdin.close()
    p.wait()
    if p.returncode != 0:
        grass.fatal(_("Cannot create the outlet vector map"))

    # convert the outlet vector map to raster
    try:
        grass.run_command("v.to.rast", overwrite=True,
                          input=outlet, output=outlet, use="cat", type="point")
    except CalledModuleError:
        grass.fatal(_("Cannot convert the outlet vector to raster"))

    # calculate the downstream flow length
    flds = prefix + "flds"
    try:
        grass.run_command("r.stream.distance", overwrite=True, flags="om",
                          stream_rast=outlet, direction=input,
                          method="downstream", distance=flds)
    except CalledModuleError:
        grass.fatal(_("Cannot calculate the downstream flow length"))

    # calculate the upstream flow length
    flus = prefix + "flus"
    try:
        grass.run_command("r.stream.distance", overwrite=True, flags="o",
                          stream_rast=outlet, direction=input,
                          method="upstream", distance=flus)
    except CalledModuleError:
        grass.fatal(_("Cannot calculate the upstream flow length"))

    # calculate the sum of downstream and upstream flow lengths
    fldsus = prefix + "fldsus"
    try:
        grass.run_command("r.mapcalc", overwrite=True,
                          expression="%s=%s+%s" % (fldsus, flds, flus))
    except CalledModuleError:
        grass.fatal(_("Cannot calculate the sum of downstream and upstream flow lengths"))

    # find the longest flow length
    p = grass.pipe_command("r.info", flags="r", map=fldsus)
    max = ""
    for line in p.stdout:
        line = line.rstrip("\n")
        if line.startswith("max="):
            max = line.split("=")[1]
            break
    p.wait()
    if p.returncode != 0 or max == "":
        grass.fatal(_("Cannot find the longest flow length"))

    min = float(max) - 0.0005

    # extract the longest flow path
    lfp = prefix + "lfp"
    try:
        grass.run_command("r.mapcalc", overwrite=True,
                          expression="%s=if(%s>=%f, 1, null())" %
                                     (lfp, fldsus, min))
    except CalledModuleError:
        grass.fatal(_("Cannot create the longest flow path raster map"))

    # thin the longest flow path raster map
    try:
        grass.run_command("r.thin", input=lfp, output=output)
    except CalledModuleError:
        grass.fatal(_("Cannot thin the longest flow path raster map"))

    # remove intermediate outputs
    grass.run_command("g.remove", flags="f", type="raster,vector",
                      pattern="%s*" % prefix)

    # write history
    tmphist = grass.tempfile()
    f = open(tmphist, "w+")
    f.write(os.environ["CMDLINE"])
    f.close()

    grass.run_command("r.support", map=output, title="Longest flow path",
                      loadhistory=tmphist, description="generated by r.lfp")
    grass.try_remove(tmphist)

if __name__ == "__main__":
    options, flags = grass.parser()
    sys.exit(main())
