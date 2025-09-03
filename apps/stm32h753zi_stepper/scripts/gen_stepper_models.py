#!/usr/bin/env python3
"""
Small generator: read stepper_models.yml and emit a C header with an array
of stepper model structs. Intended to be run at build time from CMake.
"""
import sys
import yaml
from pathlib import Path

def main(infile, outfile):
    data = yaml.safe_load(Path(infile).read_text())
    with open(outfile, 'w') as f:
        f.write('/* Auto-generated from stepper_models.yml */\n')
        f.write('#pragma once\n\n')
        f.write('#include <stdint.h>\n\n')
        f.write('typedef struct {\n')
        f.write('    const char *name;\n')
        f.write('    int steps_per_rev;\n')
        f.write('    int max_speed;\n')
        f.write('    int max_current_ma;\n')
        f.write('    int max_microstep;\n')
        f.write('    float rated_voltage;\n')
        f.write('    float holding_torque;\n')
        f.write('} stepper_model_t;\n\n')
        f.write('static const stepper_model_t stepper_models[] = {\n')
        for m in data:
            name = m.get('name','')
            f.write('    { "%s", %d, %d, %d, %d, %.3ff, %.3ff },\n' % (
                name, int(m.get('steps_per_rev',200)), int(m.get('max_speed',1000)), int(m.get('max_current',1000)), int(m.get('max_microstep',16)), float(m.get('rated_voltage',0.0)), float(m.get('holding_torque',0.0))
            ))
        f.write('};\n\n')
        f.write('static const unsigned int stepper_model_count = sizeof(stepper_models)/sizeof(stepper_models[0]);\n')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('Usage: gen_stepper_models.py <in.yml> <out.h>')
        sys.exit(2)
    main(sys.argv[1], sys.argv[2])
