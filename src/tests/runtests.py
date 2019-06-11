import os
import subprocess
import difflib

def main():
    # For every subdirectory that starts with the word test
    passed = True
    for di in os.listdir("."):
        if os.path.isdir(di):
            generate_bitcode(di)
            generate_ll_file(di)
            #passed = test_errspec_specs(di) and passed
            passed = test_errspec_bugs(di) and passed

    if passed:
        print("All tests passed.")

def generate_bitcode(test_dir):
    test_file = test_dir + "/test.c"
    bc_file = test_dir + "/test.bc"
    clang = subprocess.Popen(['clang-7', '-c', '-g', '-emit-llvm', '-fno-inline', '-O0',  test_file, '-o', bc_file])
    clang.wait()

def generate_ll_file(test_dir):
    bc_file = test_dir + "/test.bc" 
    llvm_dis = subprocess.Popen(['llvm-dis-7', bc_file])
    llvm_dis.wait()

def test_errspec_specs(test_dir):
    test_file = test_dir + "/test.bc"
    try:
        expected_f = open(test_dir + '/specs.txt', 'r')
        expected_output = "".join(expected_f.readlines())
    except:
        #print("TEST SETUP FAIL: No expected spec output for {}".format(test_file))
        return True 

    process = subprocess.Popen(
        ['../build/eesi', '--command', 'specs', '--bitcode', test_file, '--erroronly', 'test-erroronly.txt', '--inputspecs', 'test-specs.txt'], \
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    actual_output, error_output = process.communicate()
    if error_output:
        print(error_output)

    if (actual_output != expected_output):
        print("{} SPECS FAIL. Expected/Actual:".format(test_dir))
        print('\n'.join(difflib.ndiff([expected_output], [actual_output])))
        return False
    
    return True

def test_errspec_bugs(test_dir):
    test_file = test_dir + "/test.bc"
    try:
        expected_f = open(test_dir + '/bugs.txt', 'r')
        expected_output = "".join(expected_f.readlines())
    except:
        #print("TEST SETUP FAIL: No expected bugs output for {}".format(test_file))
        return True 

    specs_file = test_dir + "/specs.txt"
    process = subprocess.Popen(
        ['../build/eesi', '--command', 'bugs', '--bitcode', test_file, '--specs', specs_file, '--erroronly', 'test-erroronly.txt'], \
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    actual_output, error_output = process.communicate()
    if error_output:
        print(error_output)

    if (actual_output != expected_output):
        print("{} BUGS FAIL. Expected/Actual:".format(test_dir))
        print('\n'.join(difflib.ndiff([expected_output], [actual_output])))
        return False

    return True


if __name__ == "__main__":
    main()
