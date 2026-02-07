#!/usr/bin/env python3
"""
Script to update copyright headers in all source files to version 2.0
"""
import os
import re
from pathlib import Path

# Old header pattern to match
OLD_HEADER_PATTERN = r'/\*\s*\n \*  .*? is part of the HB-RF-ETH firmware.*?\n \*  \n \*  Copyright \d+ Alexander Reinert\s*\n \*  \n \*  The HB-RF-ETH firmware is licensed.*?\n \*  \n \*/'

# New header template
NEW_HEADER_TEMPLATE = """/*
 *  {filename} is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 5.1 and modern toolchains
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */"""

def update_file_header(filepath):
    """Update the copyright header in a file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Check if file has old header
    if 'Copyright 2022 Alexander Reinert' in content or 'Copyright 2020 Alexander Reinert' in content:
        filename = os.path.basename(filepath)
        new_header = NEW_HEADER_TEMPLATE.format(filename=filename)

        # Replace the old header
        pattern = r'/\*[\s\S]*?Copyright \d+ Alexander Reinert[\s\S]*?\*/'
        if re.search(pattern, content):
            new_content = re.sub(pattern, new_header, content, count=1)

            # Write back
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(new_content)

            print(f"Updated: {filepath}")
            return True

    return False

def main():
    """Update all source files"""
    project_root = Path(__file__).parent

    # Directories to process
    directories = [
        project_root / 'src',
        project_root / 'include',
    ]

    # File extensions to process
    extensions = ['.cpp', '.c', '.h', '.hpp']

    updated_count = 0

    for directory in directories:
        if not directory.exists():
            continue

        for ext in extensions:
            for filepath in directory.glob(f'*{ext}'):
                if update_file_header(filepath):
                    updated_count += 1

    print(f"\nTotal files updated: {updated_count}")

if __name__ == '__main__':
    main()
