#--
# = NMatrix
#
# A linear algebra library for scientific computation in Ruby.
# NMatrix is part of SciRuby.
#
# NMatrix was originally inspired by and derived from NArray, by
# Masahiro Tanaka: http://narray.rubyforge.org
#
# == Copyright Information
#
# SciRuby is Copyright (c) 2010 - 2013, Ruby Science Foundation
# NMatrix is Copyright (c) 2013, Ruby Science Foundation
#
# Please see LICENSE.txt for additional copyright notices.
#
# == Contributing
#
# By contributing source code to SciRuby, you agree to be bound by
# our Contributor Agreement:
#
# * https://github.com/SciRuby/sciruby/wiki/Contributor-Agreement
#
# == shortcuts.rb
#
# These are shortcuts for NMatrix and NVector creation, contributed by Daniel
# Carrera (dcarrera@hush.com) and Carlos Agarie (carlos.agarie@gmail.com).
#
# TODO Make all the shortcuts available through modules, allowing someone
# to include them to make "MATLAB-like" scripts.
#
# There are some questions to be answered before this can be done, tho.
#++

class NMatrix



  #
  # call-seq:
  #     dense? -> true or false
  #     list? -> true or false
  #     yale? -> true or false
  #
  # Shortcut functions for quickly determining a matrix's stype.
  #
  def dense?; return stype == :dense; end
  def yale?;  return stype == :yale;  end
  def list?;  return stype == :list;  end


  class << self
    #
    # call-seq:
    #     NMatrix[array-of-arrays, dtype = nil]
    #
    # You can use the old +N+ constant in this way:
    #   N = NMatrix
    #   N[1, 2, 3]
    # NMatrix needs to have a succinct way to create a matrix by specifying the
    # components directly. This is very useful for using it as an advanced
    # calculator, it is useful for learning how to use, for testing language
    # features and for developing algorithms.
    #
    # The NMatrix::[] method provides a way to create a matrix in a way that is compact and
    # natural. The components are specified using Ruby array syntax. Optionally,
    # one can specify a dtype as the last parameter (default is :float64).
    #
    # Examples:
    #
    #   a = NMatrix[ 1,2,3,4 ]          =>  1.0  2.0  3.0  4.0
    #
    #   a = NMatrix[ 1,2,3,4, :int32 ]  =>  1  2  3  4
    #
    #   a = NMatrix[ [1,2,3], [3,4,5] ] =>  1.0  2.0  3.0
    #                                       3.0  4.0  5.0
    #
    # SYNTAX COMPARISON:
    #
    #   MATLAB:		a = [ [1 2 3] ; [4 5 6] ]   or  [ 1 2 3 ; 4 5 6 ]
    #   IDL:			a = [ [1,2,3] , [4,5,6] ]
    #   NumPy:		a = array( [1,2,3], [4,5,6] )
    #
    #   SciRuby:      a = NMatrix[ [1,2,3], [4,5,6] ]
    #   Ruby array:   a =  [ [1,2,3], [4,5,6] ]
    #
    def [](*params)
      dtype = params.last.is_a?(Symbol) ? params.pop : nil

      # First find the dimensions of the array.
      i = 0
      shape = []
      row = params
      while row.is_a?(Array)
        shape[i] = row.length
        row = row[0]
        i += 1
      end

      # A row vector should be stored as 1xN, not N
      #shape.unshift(1) if shape.size == 1

      # Then flatten the array.
      NMatrix.new(shape, params.flatten, dtype)
    end

    #
    # call-seq:
    #    zeros(size) -> NMatrix
    #    zeros(size, dtype) -> NMatrix
    #    zeros(stype, size, dtype) -> NMatrix
    #
    # Creates a new matrix of zeros with the dimensions supplied as
    # parameters.
    #
    # * *Arguments* :
    #   - +stype+ -> (optional) Default is +:dense+.
    #   - +size+ -> Array (or integer for square matrix) specifying the dimensions.
    #   - +dtype+ -> (optional) Default is +:float64+
    # * *Returns* :
    #   - NMatrix filled with zeros.
    #
    # Examples:
    #
    #   NMatrix.zeros(2) # =>  0.0   0.0
    #                          0.0   0.0
    #
    #   NMatrix.zeros([2, 3], :int32) # =>  0  0  0
    #                                       0  0  0
    #
    #   NMatrix.zeros(:list, [1, 5], :int32) # =>  0  0  0  0  0
    #
    def zeros(*params)
      dtype = params.last.is_a?(Symbol) ? params.pop : :float64
      stype = params.first.is_a?(Symbol) ? params.shift : :dense
      dim = params.first

      NMatrix.new(stype, dim, 0, dtype)
    end
    alias :zeroes :zeros

    #
    # call-seq:
    #     ones(size) -> NMatrix
    #     ones(size, dtype) -> NMatrix
    #
    # Creates a matrix filled with ones.
    #
    # * *Arguments* :
    #   - +size+ -> Array (or integer for square matrix) specifying the dimensions.
    #   - +dtype+ -> (optional) Default is +:float64+
    # * *Returns* :
    #   - NMatrix filled with ones.
    #
    # Examples:
    #
    #   NMatrix.ones([1, 3]) # =>  1.0   1.0   1.0
    #
    #   NMatrix.ones([2, 3], :int32) # =>  1  1  1
    #                                      1  1  1
    #
    def ones(*params)
      dtype = params.last.is_a?(Symbol) ? params.pop : :float64
      dim = params.first

      NMatrix.new(dim, 1, dtype)
    end

    ##
    # call-seq:
    #   ones_like(nm) -> NMatrix
    #
    # Creates a new matrix of ones with the same dtype and shape as the
    # provided matrix.
    #
    # @param [NMatrix] nm the nmatrix whose dtype and shape will be used
    # @return [NMatrix] a new nmatrix filled with ones.
    #
    def ones_like(nm)
      NMatrix.ones(nm.shape, nm.dtype)
    end

    ##
    # call-seq:
    #   zeros_like(nm) -> NMatrix
    #
    # Creates a new matrix of zeros with the same stype, dtype, and shape
    # as the provided matrix.
    #
    # @param [NMatrix] nm the nmatrix whose stype, dtype, and shape will be used
    # @return [NMatrix] a new nmatrix filled with zeros.
    #
    def zeros_like(nm)
      NMatrix.zeros(nm.stype, nm.shape, nm.dtype)
    end

    #
    # call-seq:
    #     eye(size) -> NMatrix
    #     eye(size, dtype) -> NMatrix
    #     eye(stype, size, dtype) -> NMatrix
    #
    # Creates an identity matrix (square matrix rank 2).
    #
    # * *Arguments* :
    #   - +stype+ -> (optional) Default is +:dense+.
    #   - +size+ -> Array (or integer for square matrix) specifying the dimensions.
    #   - +dtype+ -> (optional) Default is +:float64+
    # * *Returns* :
    #   - NMatrix filled with zeros.
    #
    # Examples:
    #
    #    NMatrix.eye(3) # =>   1.0   0.0   0.0
    #                          0.0   1.0   0.0
    #                          0.0   0.0   1.0
    #
    #    NMatrix.eye(3, :int32) # =>   1   0   0
    #                                  0   1   0
    #                                  0   0   1
    #
    #    NMatrix.eye(:yale, 2, :int32) # =>   1   0
    #                                         0   1
    #
    def eye(*params)
      dtype = params.last.is_a?(Symbol) ? params.pop : :float64
      stype = params.first.is_a?(Symbol) ? params.shift : :dense

      dim = params.first

      # Fill the diagonal with 1's.
      m = NMatrix.zeros(stype, dim, dtype)
      (0...dim).each do |i|
        m[i, i] = 1
      end

      m
    end
    alias :identity :eye
    #
    # call-seq:
    #     diagonals(array) -> NMatrix
    #     diagonals(stype, array, dtype) -> NMatrix
    #     diagonals(array, dtype) -> NMatrix
    #     diagonals(stype, array) -> NMatrix
    #
    # Creates a matrix filled with specified diagonals.
    #
    # * *Arguments* :
    #   - +stype+ -> (optional) Storage type for the matrix (default is :dense)
    #   - +entries+ -> Array containing input values for diagonal matrix
    #   - +dtype+ -> (optional) Default is based on values in supplied Array
    # * *Returns* :
    #   - NMatrix filled with specified diagonal values.
    #
    # Examples:
    #
    #   NMatrix.diagonal([1.0,2,3,4]) # => 1.0 0.0 0.0 0.0
    #                                      0.0 2.0 0.0 0.0
    #                                      0.0 0.0 3.0 0.0
    #                                      0.0 0.0 0.0 4.0
    #
    #   NMatrix.diagonal(:dense, [1,2,3,4], :int32) # => 1 0 0 0
    #                                                    0 2 0 0
    #                                                    0 0 3 0
    #                                                    0 0 0 4
    #
    #
    def diagonal(*params)
      dtype = params.last.is_a?(Symbol) ? params.pop : nil
      stype = params.first.is_a?(Symbol) ? params.shift : :dense
      ary   = params.shift

      m = NMatrix.zeros(stype, ary.length, dtype || guess_dtype(ary[0]))
      ary.each_with_index do |n, i|
        m[i,i] = n
      end
      m
    end
    alias :diag :diagonal
    alias :diagonals :diagonal

    #
    # call-seq:
    #     random(size) -> NMatrix
    #
    # Creates a +:dense+ NMatrix with random numbers between 0 and 1 generated
    # by +Random::rand+. The parameter is the dimension of the matrix.
    #
    # * *Arguments* :
    #   - +size+ -> Array (or integer for square matrix) specifying the dimensions.
    # * *Returns* :
    #   - NMatrix filled with zeros.
    #
    # Examples:
    #
    #   NMatrix.random([2, 2]) # => 0.4859439730644226   0.1783195585012436
    #                               0.23193766176700592  0.4503345191478729
    #
    def random(size)
      rng = Random.new

      random_values = []

      # Construct the values of the final matrix based on the dimension.
      if size.is_a?(Integer)
        (size * size - 1).times { |i| random_values << rng.rand }
      else
        # Dimensions given by an array. Get the product of the array elements
        # and generate this number of random values.
        size.reduce(1, :*).times { |i| random_values << rng.rand }
      end

      NMatrix.new(:dense, size, random_values, :float64)
    end

    #
    # call-seq:
    #     seq(size) -> NMatrix
    #     seq(size, dtype) -> NMatrix
    #
    # Creates a matrix filled with a sequence of integers starting at zero.
    #
    # * *Arguments* :
    #   - +size+ -> Array (or integer for square matrix) specifying the dimensions.
    #   - +dtype+ -> (optional) Default is +:float64+
    # * *Returns* :
    #   - NMatrix filled with zeros.
    #
    # Examples:
    #
    #   NMatrix.seq(2) # =>   0   1
    #                 2   3
    #
    #   NMatrix.seq([3, 3], :float32) # =>  0.0  1.0  2.0
    #                                       3.0  4.0  5.0
    #                                       6.0  7.0  8.0
    #
    def seq(*params)
      dtype = params.last.is_a?(Symbol) ? params.pop : nil
      size = params.first

      # Must provide the dimension as an Integer for a square matrix or as an
      # 2 element array, e.g. [2,4].
      unless size.is_a?(Integer) || (size.is_a?(Array) && size.size < 3)
        raise ArgumentError, "seq() accepts only integers or 2-element arrays \
as dimension."
      end

      # Construct the values of the final matrix based on the dimension.
      if size.is_a?(Integer)
        values = (0 .. (size * size - 1)).to_a
      else
        # Dimensions given by a 2 element array.
        values = (0 .. (size.first * size.last - 1)).to_a
      end

      # It'll produce :int32, except if a dtype is provided.
      NMatrix.new(:dense, size, values, dtype)
    end

    #
    # call-seq:
    #     indgen(size) -> NMatrix
    #
    # Returns an integer NMatrix. Equivalent to <tt>seq(n, :int32)</tt>.
    #
    # * *Arguments* :
    #   - +size+ -> Size of the sequence.
    # * *Returns* :
    #   - NMatrix with dtype +:int32+.
    #
    def indgen(size)
      NMatrix.seq(size, :int32)
    end

    #
    # call-seq:
    #     findgen(size) -> NMatrix
    #
    # Returns a float NMatrix. Equivalent to <tt>seq(n, :float32)</tt>.
    #
    # * *Arguments* :
    #   - +size+ -> Size of the sequence.
    # * *Returns* :
    #   - NMatrix with dtype +:float32+.
    #
    def findgen(size)
      NMatrix.seq(size, :float32)
    end

    #
    # call-seq:
    #     bindgen(size) -> NMatrix
    #
    # Returns a byte NMatrix. Equivalent to <tt>seq(n, :byte)</tt>.
    #
    # * *Arguments* :
    #   - +size+ -> Size of the sequence.
    # * *Returns* :
    #   - NMatrix with dtype +:byte+.
    #
    def bindgen(size)
      NMatrix.seq(size, :byte)
    end

    #
    # call-seq:
    #     cindgen(size) -> NMatrix
    #
    # Returns an complex NMatrix. Equivalent to <tt>seq(n, :complex64)</tt>.
    #
    # * *Arguments* :
    #   - +size+ -> Size of the sequence.
    # * *Returns* :
    #   - NMatrix with dtype +:complex64+.
    #
    def cindgen(size)
      NMatrix.seq(size, :complex64)
    end

  end
end

module NVector

  class << self
    #
    # call-seq:
    #     new(shape) -> NVector
    #     new(stype, shape) -> NVector
    #     new(shape, init) -> NVector
    #     new(:dense, shape, init) -> NVector
    #     new(:list, shape, init) -> NVector
    #     new(shape, init, dtype) -> NVector
    #     new(stype, shape, init, dtype) -> NVector
    #     new(stype, shape, dtype) -> NVector
    #
    # Creates a new NVector. See also NMatrix#initialize for a more detailed explanation of
    # the arguments.
    #
    # * *Arguments* :
    #   - +stype+ -> (optional) Storage type of the vector (:list, :dense, :yale). Defaults to :dense.
    #   - +shape+ -> Shape of the vector. Accepts [n,1], [1,n], or n, where n is a Fixnum.
    #   - +init+ -> (optional) Yale: capacity; List: default value (0); Dense: initial value or values (uninitialized by default).
    #   - +dtype+ -> (optional if +init+ provided) Data type stored in the vector. For :dense and :list, can be inferred from +init+.
    # * *Returns* :
    #   -
    #
    def new(*args)
      stype = args[0].is_a?(Symbol) ? args.shift : :dense
      shape = args[0].is_a?(Array) ? args.shift  : [1,args.shift]

      if shape.size != 2 || !shape.include?(1) || shape == [1,1]
        raise(ArgumentError, "shape must be a Fixnum or an Array of positive Fixnums where exactly one value is 1")
      end

      NMatrix.new(stype, shape, *args)
    end

    #
    # call-seq:
    #     zeros(size) -> NMatrix
    #     zeros(size, dtype) -> NMatrix
    #
    # Creates a new vector of zeros with the dimensions supplied as
    # parameters.
    #
    # * *Arguments* :
    #   - +size+ -> Array (or integer for square matrix) specifying the dimensions.
    #   - +dtype+ -> (optional) Default is +:float64+.
    # * *Returns* :
    #   - NVector filled with zeros.
    #
    # Examples:
    #
    #   NVector.zeros(2) # =>  0.0
    #                          0.0
    #
    #   NVector.zeros(3, :int32) # =>  0
    #                                  0
    #                                  0
    #
    def zeros(size, dtype = :float64)
      NVector.new(size, 0, dtype)
    end
    alias :zeroes :zeros

    #
    # call-seq:
    #     ones(size) -> NVector
    #     ones(size, dtype) -> NVector
    #
    # Creates a vector of ones with the dimensions supplied as
    # parameters.
    #
    # * *Arguments* :
    #   - +size+ -> Array (or integer for square matrix) specifying the dimensions.
    #   - +dtype+ -> (optional) Default is +:float64+.
    # * *Returns* :
    #   - NVector filled with ones.
    #
    # Examples:
    #
    #   NVector.ones(2) # =>  1.0
    #                         1.0
    #
    #   NVector.ones(3, :int32) # =>  1
    #                                 1
    #                                 1
    #
    def ones(size, dtype = :float64)
      NVector.new(size, 1, dtype)
    end

    #
    # call-seq:
    #     random(size) -> NVector
    #
    # Creates a vector with random numbers between 0 and 1 generated by
    # +Random::rand+ with the dimensions supplied as parameters.
    #
    # * *Arguments* :
    #   - +size+ -> Array (or integer for square matrix) specifying the dimensions.
    #   - +dtype+ -> (optional) Default is +:float64+.
    # * *Returns* :
    #   - NVector filled with random numbers generated by the +Random+ class.
    #
    # Examples:
    #
    #   NVector.rand(2) # =>  0.4859439730644226
    #                         0.1783195585012436
    #
    def random(size)
      rng = Random.new

      random_values = []
      size.times { |i| random_values << rng.rand }

      NVector.new(size, random_values, :float64)
    end

    #
    # call-seq:
    #     seq(n) -> NVector
    #     seq(n, dtype) -> NVector
    #
    # Creates a vector with a sequence of +n+ integers starting at zero. You
    # can choose other types based on the dtype parameter.
    #
    # * *Arguments* :
    #   - +n+ -> Number of integers in the sequence.
    #   - +dtype+ -> (optional) Default is +:int64+.
    # * *Returns* :
    #   - NVector filled with +n+ integers.
    #
    # Examples:
    #
    #   NVector.seq(2) # =>  0
    #                        1
    #
    #   NVector.seq(3, :float32) # =>  0.0
    #                                  1.0
    #                                  2.0
    #
    def seq(size, dtype = :int64)
      values = (0 ... size).to_a

      NVector.new(size, values, dtype)
    end

    #
    # call-seq:
    #     indgen(n) -> NVector
    #
    # Returns an integer NVector. Equivalent to <tt>seq(n, :int32)</tt>.
    #
    # * *Arguments* :
    #   - +n+ -> Size of the sequence.
    # * *Returns* :
    #   - NVector filled with +n+ integers of dtype +:int32+.
    #
    def indgen(n)
      NVector.seq(n, :int32)
    end

    #
    # call-seq:
    #     findgen(n) -> NVector
    #
    # Returns a float NVector. Equivalent to <tt>seq(n, :float32)</tt>.
    #
    # * *Arguments* :
    #   - +n+ -> Size of the sequence.
    # * *Returns* :
    #   - NVector filled with +n+ integers of dtype +:float32+.
    #
    def findgen(n)
      NVector.seq(n, :float32)
    end

    #
    # call-seq:
    #     bindgen(n) -> NVector
    #
    # Returns a byte NVector. Equivalent to <tt>seq(n, :byte)</tt>.
    #
    # * *Arguments* :
    #   - +n+ -> Size of the sequence.
    # * *Returns* :
    #   - NVector filled with +n+ integers of dtype +:byte+.
    #
    def bindgen(n)
      NVector.seq(n, :byte)
    end

    #
    # call-seq:
    #     cindgen(n) -> NVector
    #
    # Returns a complex NVector. Equivalent to <tt>seq(n, :complex64)</tt>.
    #
    # * *Arguments* :
    #   - +n+ -> Size of the sequence.
    # * *Returns* :
    #   - NVector filled with +n+ integers of dtype +:complex64+.
    #
    def cindgen(n)
      NVector.seq(n, :complex64)
    end

    #
    # call-seq:
    #     linspace(a, b) -> NVector
    #     linspace(a, b, n) -> NVector
    #
    # Returns a NVector with +n+ values of dtype +:float64+ equally spaced from
    # +a+ to +b+, inclusive.
    #
    # See: http://www.mathworks.com/help/matlab/ref/linspace.html
    #
    # * *Arguments* :
    #   - +a+ -> The first value in the sequence.
    #   - +b+ -> The last value in the sequence.
    #   - +n+ -> The number of elements. Default is 100.
    # * *Returns* :
    #   - NVector with +n+ +:float64+ values.
    #
    # Example:
    #   x = NVector.linspace(0, Math::PI, 1000)
    #   x.pretty_print
    #     [0.0
    #     0.0031447373909807737
    #     0.006289474781961547
    #     ...
    #     3.135303178807831
    #     3.138447916198812
    #     3.141592653589793]
    #   => nil
    #
    def linspace(a, b, n = 100)
      # Formula: seq(n) * step + a

      # step = ((b - a) / (n - 1))
      step = (b - a) * (1.0 / (n - 1))

      # dtype = :float64 is used to prevent integer coercion.
      result = NVector.seq(n, :float64) * NVector.new(n, step, :float64)
      result += NVector.new(n, a, :float64)
      result
    end

    #
    # call-seq:
    #     logspace(a, b) -> NVector
    #     logspace(a, b, n) -> NVector
    #
    # Returns a NVector with +n+ values of dtype +:float64+ logarithmically
    # spaced from +10^a+ to +10^b+, inclusive.
    #
    # See: http://www.mathworks.com/help/matlab/ref/logspace.html
    #
    # * *Arguments* :
    #   - +a+ -> The first value in the sequence.
    #   - +b+ -> The last value in the sequence.
    #   - +n+ -> The number of elements. Default is 100.
    # * *Returns* :
    #   - NVector with +n+ +:float64+ values.
    #
    # Example:
    #   x = NVector.logspace(0, Math::PI, 10)
    #   x.pretty_print
    #     [1.0
    #     2.2339109164570266
    #     4.990357982665873
    #     11.148015174505757
    #     24.903672795156997
    #     55.632586516975095
    #     124.27824233101062
    #     277.6265222213364
    #     620.1929186882427
    #     1385.4557313670107]
    #  => nil
    #
    def logspace(a, b, n = 100)
      # Formula: 10^a, 10^(a + step), ..., 10^b, where step = ((b-a) / (n-1)).

      result = NVector.linspace(a, b, n)
      result.each_stored_with_index { |element, i| result[i] = 10 ** element }
      result
    end
  end
end


# Use this constant as you would use NMatrix[].
# Examples:
#
#   a = N[ 1,2,3,4 ]          =>  1.0  2.0  3.0  4.0
#
#   a = N[ 1,2,3,4, :int32 ]  =>  1  2  3  4
#
#   a = N[ [1,2,3], [3,4,5] ] =>  1.0  2.0  3.0
#                                 3.0  4.0  5.0
#
N = NMatrix
