def file_to_c_hex(filename, bytes_per_line=16):
    """Converts a file to a C-style hex string."""
    with open(filename, 'rb') as f:
        data = f.read()
    
    hex_list = [f"0x{b:02X}" for b in data]
    lines = []
    
    for i in range(0, len(hex_list), bytes_per_line):
        line = ", ".join(hex_list[i : i + bytes_per_line])
        # Add a comma at the end of each line
        lines.append(f"    {line},")
        
    return "\n".join(lines), len(data)



def generate_header_file(input_file, output_h_file, variable_name):
    """Generates a .h file with PGMSPACE formatting for Arduino/C++."""
    # Get the hex string and the actual byte length
    hex_content, file_len = file_to_c_hex(input_file)
    
    header_template = f"""#include <pgmspace.h>

const unsigned char {variable_name}[] PROGMEM = {{
{hex_content}
}};
unsigned int {variable_name}_len = {file_len};
"""

    with open(output_h_file, 'w') as f:
        f.write(header_template)
    
    print(f"Success! Created {output_h_file} with variable '{variable_name}'")


generate_header_file("samples_all/Trimmed Samples/Blues/kick.wav", "kick.h", "kick_wav")