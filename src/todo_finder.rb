DIRS = ["devices", "lib", "threads"]
IGNORED_DIRS = [/\./, /\.\./, /build/]
FILE_PATTERNS = [/\.c$/, /\.h$/]

MAX_LINE_LENGTH = 79
PERSON_PATTERN = /\((.*) \d{4}-\d{2}-\d{2}/
PERSON_INDEX = 1
IGNORED_PEOPLE = [/Mark Rutland/]

TODO_START_PATTERN = /(\/\/|\/\*)\s*TODO\s*-\s*/i
TODO_END_PATTERN = /\*\//

def check_directory(dir = ".")
  Dir.foreach(dir) do |file_name|
    file_with_path = dir + "/" + file_name

    if File.directory?(file_with_path)
      if IGNORED_DIRS.all? {|ignored_dir| file_name !~ ignored_dir}
        check_directory(file_with_path)
      end
    elsif FILE_PATTERNS.any? {|file_pattern| file_name =~ file_pattern}
      check_file file_with_path
    end
  end
end

def check_file(file_name)
  line_no = 1
  File.foreach(file_name) do |line|
    if line =~ /TODO/i
      todo = line.strip.gsub(TODO_START_PATTERN, "").gsub(TODO_END_PATTERN, "")
      puts "#{file_name}:#{line_no} TODO - #{todo}"
    end

    if line.length > MAX_LINE_LENGTH
      person = `git blame -L #{line_no},+1 #{file_name}`
      person = PERSON_PATTERN.match(person)[PERSON_INDEX]
      if IGNORED_PEOPLE.all? {|ignored| person !~ ignored}
        puts "#{file_name}:#{line_no} is too long, blame #{person}"
      end
    end

#if line =~ /^\s*[\w_]+\(/
#     puts "#{file_name}:#{line_no} bad method call"
#   end

    line_no += 1
  end
end

DIRS.each {|dir| check_directory dir }
