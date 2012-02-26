DIRS = ["devices", "lib", "threads", "userprog"]
IGNORED_DIRS = [/\./, /\.\./, /build/]
FILE_PATTERNS = [/\.c$/, /\.h$/, /^DESIGNDOC$/]

MAX_LINE_LENGTH = 79
PERSON_PATTERN = /\((.*) \d{4}-\d{2}-\d{2}/
PERSON_INDEX = 1
IGNORED_PEOPLE = [/Mark Rutland/]

TODO_START_PATTERN = /(\/\/|\/\*)\s*TODO\s*[-:]\s*/i
TODO_END_PATTERN = /\*\//

class TodoFinder

  def initialize(strategy)
    @strategy = strategy
    DIRS.each {|dir| check_directory(dir) }
  end

  def check_directory(dir)
    Dir.foreach(dir) do |file_name|
      file_name_with_path = dir + "/" + file_name
      if File.directory?(file_name_with_path)
        # Directory
        if IGNORED_DIRS.all? {|ignored_dir| file_name !~ ignored_dir}
          check_directory(file_name_with_path)
        end
      elsif FILE_PATTERNS.any? {|file_pattern| file_name =~ file_pattern}
        # Wanted file
        check_file(file_name_with_path)
      end
    end
  end

  def check_file(file_name)
    line_no = 1
    File.foreach(file_name) do |line|
      @strategy.check_line(file_name, line_no, line.chomp!)

      #if line =~ /[\w_]+\([^;)]*\)[;,]/
      #     puts "#{file_name}:#{line_no} bad method call"
      #   end

      line_no += 1
    end
  end

end

class AnalyzingStrategy
  def check_line(file_name, line_no, line)
    if line =~ /TODO/i
      todo = line.strip.gsub(TODO_START_PATTERN, "").gsub(TODO_END_PATTERN, "")
      puts "#{file_name}:#{line_no} TODO - #{todo}"
    end

    if line.length > MAX_LINE_LENGTH
      villain = get_villain(file_name, line_no)
      if IGNORED_PEOPLE.all? {|ignored| villain !~ ignored}
        puts "#{file_name}:#{line_no} is too long (#{line.length} chars), blame #{villain}"
      end
    end
  end
  
  def get_villain(file_name, line_no)
    villain = `git blame -L #{line_no},+1 #{file_name}`
    PERSON_PATTERN.match(villain)[PERSON_INDEX]
  end
end

class SearchingStrategy
  def initialize(regex)
    @regex = regex
  end

  def check_line(file_name, line_no, line)
    matches = @regex.match(line)
    if matches
      puts "#{file_name}:#{line_no} #{matches[1].lstrip + matches[2] + matches[3].rstrip}"
    end
  end
end

if ARGV.length == 1
  regex = /(.{0,10})(#{ARGV[0]})(.{0,10})/
  strategy = SearchingStrategy.new(regex)
else
  strategy = AnalyzingStrategy.new
end

TodoFinder.new(strategy)

