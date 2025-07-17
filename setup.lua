import("core.base.scheduler")
import("private.async.jobpool")
import("async.runjobs")

local function is_empty_folder(dir)
    if os.exists(dir) and not os.isfile(dir) then
        for _, v in ipairs(os.filedirs(path.join(dir, '*'))) do
            return false
        end
        return true
    else
        return true
    end
end
local function git_clone_or_pull(git_address, subdir, branch)
    local args
    if is_empty_folder(subdir) then
        args = {'clone', git_address}
        if branch then
            table.insert(args, '-b')
            table.insert(args, branch)
        end
        table.insert(args, path.translate(subdir))
    else
        args = {'-C', subdir, 'pull'}
    end
    local done = false
    print("pulling " .. git_address)
    for i = 1, 4, 1 do
        try {function()
            os.execv('git', args)
            done = true
        end}
        if done then
            return
        end
    end
    utils.error("git clone error.")
    os.exit(1)
end

function install_lc(llvm_path)
    local lc_path = "LuisaCompute"
    ------------------------------ llvm ------------------------------
    local builddir
    if os.is_host("windows") then
        builddir = path.absolute("build/windows/x64/release")
    elseif os.is_host("macosx") then
        builddir = path.absolute("build/macosx/x64/release")
    end
    local lib = import("lib", {
        rootdir = "LuisaCompute/scripts"
    })
    if llvm_path then
        if not llvm_path or type(llvm_path) ~= "string" then
            utils.error("Bad argument, should be 'xmake l setup.lua #LLVM_PATH#'")
            os.exit(1)
        elseif not os.isdir(llvm_path) then
            utils.error("LLVM path illegal")
            os.exit(1)
        end

        print("copying llvm...")

        local llvm_code_path = path.absolute("src/clangcxx/llvm", lc_path)
        os.tryrm(path.join(llvm_code_path, "include"))
        os.tryrm(path.join(llvm_code_path, "lib"))
        os.cp(path.join(llvm_path, "include"), path.join(llvm_code_path, "include"))
        os.cp(path.join(llvm_path, "lib"), path.join(llvm_code_path, "lib"))
        if builddir then
            lib.mkdirs(builddir)
            if os.is_host("windows") then
                os.cp(path.join(llvm_path, "bin", "clang.exe"), builddir)
            elseif os.is_host("macosx") then
                os.cp(path.join(llvm_path, "bin", "clang"), builddir)
            end
        else
            utils.error("build dir not set.")
            os.exit(1)
        end
    end

    print("preparing shader command...")
    local shader_out_dir = path.translate(path.join(path.directory(builddir), "shader_build"))
    local compiler_path = "clangcxx_compiler"
    if os.is_host("windows") then
        compiler_path = compiler_path .. ".exe"
    end
    compiler_path = path.join(os.projectdir(), "build", os.host(), os.arch(), "release", compiler_path)
    local shader_dir = path.translate(path.absolute("shaders/"))
    local compile_cmd_path = path.translate(path.join(shader_dir, 'compile.cmd'))
    local gen_json_cmd_path = path.translate(path.join(shader_dir, 'gen_json.cmd'))
    local sb = lib.StringBuilder()
    sb:add('@echo off\n"'):add(compiler_path):add('" --in="'):add(shader_dir):add('\\src" --out="'):add(shader_out_dir):add('" --include="'):add(shader_dir):add('\\include"')
    sb:write_to(compile_cmd_path)
    compile_cmd_path = path.translate(path.join(shader_dir, 'clean_compile.cmd'))
    sb:add(' --rebuild')
    sb:write_to(compile_cmd_path)
    sb:clear()
    sb:add('@echo off\n"'):add(compiler_path):add('" --in="'):add(shader_dir):add('\\src" --out="'):add(shader_dir):add(
        '\\.vscode\\compile_commands.json" --include="'):add(shader_dir):add('\\include"'):add(' --lsp')
    sb:write_to(gen_json_cmd_path)
    sb:dispose()
end

function main(llvm_path)
    install_lc(llvm_path)
end
