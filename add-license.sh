#!/bin/bash

for file in $(find $(dirname $0)/includes -name "*.hpp" -type f); do
    cat <<EOF >> "${file}.tmp"
/*
* Copyright (c) 2023 Zach Gerstman
* https://github.com/crabmandable/zxorm
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/
EOF
    cat "$file" >> "$file.tmp"
    mv "$file.tmp" "$file"
done

